#include "ndr.h"
#include "video.h"
#include "kernel.h"
#include "memory.h"
#include "acpi.h"

typedef struct {
    UINT64* pml4;
    UINT64  stackCount;
    VIDEO_FRAMEBUFFER fb;
    KERNEL_INFO kInfo;
    RSDP* rsdp;
    MEMORY_MAP map;
} BOOT_INFO;

CHAR16* registryFileAddress = L"\\NEODOS\\OSDATA.NDR";

VOID EFIAPI errorHandler(IN EFI_STATUS Status, IN EFI_HANDLE ImageHandle) {
    uefi_call_wrapper(ST->ConOut->ClearScreen, 1, ST->ConOut);
    uefi_call_wrapper(ST->ConOut->SetCursorPosition, 3, ST->ConOut, 1, 1);
    Print(L"\n");
    Print(L"================================\n");
    Print(L"NeoDOS Boot Error\n");
    Print(L"================================\n\n");

    Print(L"Error Name: %r\n\n", Status);

    Print(L"NeoDOS Bootloader encountered an unexpected error and cannot continue.\n");
    Print(L"Please check your system configuration or firmware settings, and try rebooting.\n\n");

    Print(L"System halted. Waiting 10 seconds...\n");

    uefi_call_wrapper(BS->Stall, 1, 10 * 1000 * 1000);
    uefi_call_wrapper(BS->Exit, 4, ImageHandle, Status, 0, NULL);
}

EFI_STATUS EFIAPI efi_main(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE* SystemTable) {
    InitializeLib(ImageHandle, SystemTable);
    BOOT_INFO bInfo;

    Print(L"Hello, World!\n");

    ConfigNode* tree;
    EFI_STATUS Status = getConfig(registryFileAddress, &tree);
    if (EFI_ERROR(Status)) {
        Print(L"ERROR: (config) Unexpected error: %r\n", Status);
        errorHandler(Status, ImageHandle);
    }

    VIDEO_INFO vInfo = {
        .handle = NULL,
        .resolution = {
            .height = 0,
            .width = 0,
        },
    };
    Status = getPreferredResolution(&vInfo);
    if (EFI_ERROR(Status) && Status != EFI_NOT_FOUND) errorHandler(Status, ImageHandle);

    if (Status == EFI_NOT_FOUND) {
        vInfo.resolution.height = (UINT16)ConfigNodeGetU64(&tree[0], (CHAR8*)"SystemConfig/Height", 1920);
        vInfo.resolution.width = (UINT16)ConfigNodeGetU64(&tree[0], (CHAR8*)"SystemConfig/Width", 1080);
    }

    Print(L"INFO: (video) Using resolution %dx%d\n", vInfo.resolution.width, vInfo.resolution.height);

    Status = loadElf(ConfigNodeGetStr16(&tree[0], (CHAR8*)"Bootloader/KernelLocation", L"\\NEODOS\\KERNEL.BIN"), &bInfo.kInfo);
    if (EFI_ERROR(Status)) errorHandler(Status, ImageHandle);

    Status = findACPI(&bInfo.rsdp);
    if (EFI_ERROR(Status)) errorHandler(Status, ImageHandle);

    UINTN maxCPU = ConfigNodeGetU64(&tree[0], (CHAR8*)"Bootloader/MaxCPU", 4);
    bInfo.stackCount = maxCPU;

    Status = setVideoMode(vInfo, &bInfo.fb);
    if (EFI_ERROR(Status)) errorHandler(Status, ImageHandle);

    Status = initPage(&bInfo.pml4);
    if (EFI_ERROR(Status)) errorHandler(Status, ImageHandle);

    Status = mapKernelSpace(bInfo.pml4, &bInfo.kInfo, &bInfo.fb, maxCPU);
    if (EFI_ERROR(Status)) errorHandler(Status, ImageHandle);

    Status = getMemoryMap(&bInfo.map);
    if (EFI_ERROR(Status)) errorHandler(Status, ImageHandle);

    CopyMem((VOID*)bInfo.kInfo.bootInfo.paddr, (VOID*)&bInfo, sizeof(bInfo));

    return EFI_SUCCESS;
}