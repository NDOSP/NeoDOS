#include "syscalls.h"
#include "scheduler/scheduler.h"
#include "ipc/ipc.h"
#include "memory/pmm.h"
#include "memory/vmm.h"
#include "memory/paging.h"
#include "memory/memutils.h"
#include "modman/modman.h"
#include "shm/shm.h"
#include "bootinfo.h"
#include "serial.h"

extern void syscall_entry(void); 
PerCpuData perCpuArray[255];

void initSyscalls(uint64_t cpuId, void* kStack) {
    perCpuArray[cpuId].cpuId = cpuId;
    perCpuArray[cpuId].kernelStack = kStack;

    uint64_t addr = (uint64_t)&perCpuArray[cpuId];
    asm volatile("wrmsr" : : "a"((uint32_t)addr), "d"((uint32_t)(addr >> 32)), "c"(MSR_KERNEL_GS_BASE));

    uint32_t low, high;
    asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(MSR_EFER));
    low |= (1 << 11) | 1;
    asm volatile("wrmsr" : : "a"(low), "d"(high), "c"(MSR_EFER));

    uint64_t star = ((uint64_t)0x10 << 32) | ((uint64_t)0x18 << 48);
    asm volatile("wrmsr" : : "a"((uint32_t)star), "d"((uint32_t)(star >> 32)), "c"(MSR_STAR));

    uint64_t lstar = (uint64_t)syscall_entry;
    asm volatile("wrmsr" : : "a"((uint32_t)lstar), "d"((uint32_t)(lstar >> 32)), "c"(MSR_LSTAR));

    asm volatile("wrmsr" : : "a"(0x200), "d"(0), "c"(MSR_SFMASK));
}

uint64_t syscallDispatcher(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, SyscallFrame* sf) {
    (void)sf;
    (void)arg4;

    switch (num) {
    case SYSCALL_EXIT:
        exitTask();
        return 0;

    case SYSCALL_FORK: {
        uint64_t child = forkTask(sf);
        return child;
    }

    case SYSCALL_SEND: {
        uint64_t buf[IPC_MSG_SIZE / 8];
        memcpy((void*)buf, (void*)arg2, IPC_MSG_SIZE);
        uint64_t sender = getCurrentPid();
        return ipcSendPid(arg1, sender, buf);
    }

    case SYSCALL_RECV: {
        uint64_t senderPid;
        uint64_t buf[IPC_MSG_SIZE / 8];
        int ret = ipcRecv(buf, &senderPid);
        if (ret == 0) {
            memcpy((void*)arg1, (void*)buf, IPC_MSG_SIZE);
            return senderPid;
        }
        return schedulerBlockAndSwitch(sf);
    }

    case SYSCALL_RECV_FROM: {
        uint64_t expected = arg1;
        uint64_t senderPid;
        uint64_t buf[IPC_MSG_SIZE / 8];
        int ret = ipcRecvFrom(buf, &senderPid, expected);
        if (ret == 0) {
            memcpy((void*)arg2, (void*)buf, IPC_MSG_SIZE);
            return senderPid;
        }
        return schedulerBlockAndSwitch(sf);
    }

    case SYSCALL_GETPID:
        return getCurrentPid();

    case SYSCALL_ALLOC_PAGES: {
        uint64_t n = arg1;
        uint64_t vaddr = arg2;
        if (n == 0 || n > 256) return -1;
        uint64_t size = n * PAGE_SIZE;

        void* phys = pmmAllocator(n);
        if (!phys) return -1;
        uint64_t paddr = (uint64_t)phys;

        Task* tsk = getCurrentTask();
        if (!tsk) { pmmFree(phys, n); return -1; }

        if (vaddr == 0) {
            vaddr = tsk->vaddr_next;
            tsk->vaddr_next += size;
        }

        if (tsk->cr3) {
            vmm_map_in_cr3(tsk->cr3, vaddr, size, paddr,
                           PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
        } else {
            addPageRange(vaddr, size, paddr, PAGE_PRESENT | PAGE_WRITE);
        }

        for (uint64_t off = 0; off < size; off += PAGE_SIZE) {
            void* tmp = tempMap((void*)(paddr + off));
            memset(tmp, 0, PAGE_SIZE);
            tempUnmap();
        }
        return vaddr;
    }

    case SYSCALL_FREE_PAGES: {
        uint64_t phys = arg1;
        uint64_t n = arg2;
        if (n == 0 || n > 256) return -1;
        freePages((void*)phys, n);
        pmmFree((void*)phys, n);
        return 0;
    }

    case SYSCALL_SHM: {
        uint64_t sub = arg1;
        switch (sub) {
        case SHM_CREATE: {
            uint64_t pages = arg2;
            return shm_create(pages);
        }
        case SHM_ATTACH: {
            uint64_t handle = arg2;
            uint64_t vaddr;
            if (shm_attach(handle, &vaddr) == 0)
                return vaddr;
            return -1ULL;
        }
        case SHM_DETACH: {
            uint64_t handle = arg2;
            return shm_detach(handle);
        }
        default:
            return -1ULL;
        }
    }

    case SYSCALL_WRITE: {
        char buf[256];
        uint64_t len = arg2 > 255 ? 255 : arg2;
        memcpy(buf, (void*)arg1, len);
        buf[len] = '\0';
        serial_puts(buf);
        return len;
    }

    case SYSCALL_MOD_LIST: {
        ModEntry* entries = (ModEntry*)arg1;
        int max = (int)arg2;
        return modman_list(entries, max);
    }

    case SYSCALL_MOD: {
        Task* task = getCurrentTask();
        if (!task || !task->isModule) return -1ULL;

        uint64_t sub = arg1;

        switch (sub) {
        case 1: { // PHYS_MAP(paddr, pages, vaddr)  vaddr=0 → identity
            uint64_t paddr = arg2 & ~0xFFFULL;
            uint64_t pages = arg3;
            uint64_t vaddr = arg4;
            if (pages == 0 || pages > 65536) return -1ULL;
            if (vaddr == 0) vaddr = paddr; // identity map
            uint64_t size = pages * PAGE_SIZE;
            void* ret = addPageRange(vaddr, size, paddr,
                                     PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
            if (ret && task->cr3) {
                vmm_map_in_cr3(task->cr3, vaddr, size, paddr,
                               PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
            }
            return ret ? vaddr : -1ULL;
        }

        case 2: { // BOOTINFO
            uint64_t type = arg2 & 0xFFULL;
            uint64_t extra = (arg2 >> 8) & 0xFFFFFFULL;
            uint64_t max_size = arg2 >> 32;
            void* out = (void*)arg3;

            switch (type) {
            case 1: { // Framebuffer info
                struct {
                    uint64_t addr;
                    uint64_t size;
                    uint32_t width;
                    uint32_t height;
                    uint32_t scanline;
                    uint32_t format;
                } fb = {
                    (uint64_t)bInfo.fb.fbPtr,
                    bInfo.fb.fbSize,
                    bInfo.fb.fbWidth,
                    bInfo.fb.fbHeight,
                    bInfo.fb.fbScanlineBytes,
                    (uint32_t)bInfo.fb.pixelFormat
                };
                uint64_t copy = sizeof(fb) < max_size ? sizeof(fb) : max_size;
                memcpy(out, &fb, copy);
                return copy;
            }
            case 2: { // Font info
                struct {
                    uint64_t addr;
                    uint32_t width;
                    uint32_t height;
                    uint32_t glyph_count;
                    uint32_t bytes_per_glyph;
                } font;
                if (!bInfo.font) return -1ULL;
                font.addr = (uint64_t)bInfo.font;
                font.width = bInfo.font->fontWidth;
                font.height = bInfo.font->fontHeight;
                font.glyph_count = bInfo.font->glyphCount;
                font.bytes_per_glyph = bInfo.font->bytesPerGlyph;
                uint64_t copy = sizeof(font) < max_size ? sizeof(font) : max_size;
                memcpy(out, &font, copy);
                return copy;
            }
            case 3: { // Module count
                uint64_t count = bInfo.moduleCount;
                if (max_size >= sizeof(count)) {
                    memcpy(out, &count, sizeof(count));
                    return sizeof(count);
                }
                return -1ULL;
            }
            case 4: { // Module info by index (extra = index)
                uint64_t idx = extra;
                if (idx >= bInfo.moduleCount) return -1ULL;
                struct {
                    uint64_t addr;
                    uint64_t size;
                    char name[32];
                } mod;
                mod.addr = (uint64_t)bInfo.modules[idx].data;
                mod.size = bInfo.modules[idx].size;
                for (int i = 0; i < 32; i++)
                    mod.name[i] = bInfo.modules[idx].name[i];
                uint64_t copy = sizeof(mod) < max_size ? sizeof(mod) : max_size;
                memcpy(out, &mod, copy);
                return copy;
            }
            default:
                return -1ULL;
            }
        }

        case 3: { // BOOTINFO_SIZE
            uint64_t type = arg2;
            switch (type) {
            case 1: return 40;
            case 2: return 28;
            case 3: return 8;
            case 4: return 48;
            default: return -1ULL;
            }
        }

        case 4: { // PORT_IO
            uint16_t port = arg2 & 0xFFFF;
            uint8_t width = (arg2 >> 16) & 0xFF;
            uint8_t dir = (arg2 >> 24) & 0xFF;
            uint32_t value = arg3 & 0xFFFFFFFF;

            if (dir == 0) { // in
                switch (width) {
                case 8: {
                    uint8_t v;
                    asm volatile("inb %1, %0" : "=a"(v) : "Nd"(port));
                    return v;
                }
                case 16: {
                    uint16_t v;
                    asm volatile("inw %1, %0" : "=a"(v) : "Nd"(port));
                    return v;
                }
                case 32: {
                    uint32_t v;
                    asm volatile("inl %1, %0" : "=a"(v) : "Nd"(port));
                    return v;
                }
                }
            } else { // out
                switch (width) {
                case 8:
                    asm volatile("outb %0, %1" : : "a"((uint8_t)value), "Nd"(port));
                    return 0;
                case 16:
                    asm volatile("outw %0, %1" : : "a"((uint16_t)value), "Nd"(port));
                    return 0;
                case 32:
                    asm volatile("outl %0, %1" : : "a"(value), "Nd"(port));
                    return 0;
                }
            }
            return -1ULL;
        }

        case 5: { // PHYS_ADDR(vaddr) → paddr
            uint64_t vaddr = arg2;
            uint64_t paddr = vmtoPm(vaddr);
            return paddr ? paddr : -1ULL;
        }

        case 6: { // REP_INSW(port, buf_phys, words)
            uint16_t port = arg2 & 0xFFFF;
            uint64_t buf_phys = arg3;
            uint64_t words = arg4;
            if (words == 0 || words > 65536) return -1ULL;
            uint64_t off = 0;
            while (words > 0) {
                uint64_t page_off = (buf_phys + off) & 0xFFF;
                uint64_t paddr = (buf_phys + off) & ~0xFFFULL;
                void* vaddr = tempMap((void*)paddr);
                uint64_t chunk = (PAGE_SIZE - page_off) / 2;
                if (chunk > words) chunk = words;
                asm volatile("rep insw" : : "d"(port), "D"((uint8_t*)vaddr + page_off), "c"(chunk) : "memory");
                tempUnmap();
                off += chunk * 2;
                words -= chunk;
            }
            return 0;
        }

        case 7: { // REP_OUTSW(port, buf_phys, words)
            uint16_t port = arg2 & 0xFFFF;
            uint64_t buf_phys = arg3;
            uint64_t words = arg4;
            if (words == 0 || words > 65536) return -1ULL;
            uint64_t off = 0;
            while (words > 0) {
                uint64_t page_off = (buf_phys + off) & 0xFFF;
                uint64_t paddr = (buf_phys + off) & ~0xFFFULL;
                void* vaddr = tempMap((void*)paddr);
                uint64_t chunk = (PAGE_SIZE - page_off) / 2;
                if (chunk > words) chunk = words;
                asm volatile("rep outsw" : : "d"(port), "S"((uint8_t*)vaddr + page_off), "c"(chunk) : "memory");
                tempUnmap();
                off += chunk * 2;
                words -= chunk;
            }
            return 0;
        }

        default:
            return -1ULL;
        }
    }

    default:
        return -1;
    }
}