#include "modlib.h"

#define VID_CLEAR     4
#define VID_PUTSTR    2
#define VID_PUTCHAR   1
#define VID_SETSCALE  5

#define KBD_GETCHAR   1

#define COL_WHITE 0x00FFFFFF

#define FONT_W 8
#define FONT_H 13
#define LINE_MAX 256
#define PROMPT "C:\\> "
#define PROMPT_LEN 5
#define TEXT_SCALE 2

typedef struct { uint64_t pid; char name[64]; } ModEntry;

static uint64_t find_mod(const char* name) {
    ModEntry entries[16];
    int n = mod_list(entries, 16);
    for (int i = 0; i < n; i++) {
        int match = 1;
        for (int j = 0; j < 63; j++) {
            if (entries[i].name[j] != name[j]) { match = 0; break; }
            if (name[j] == '\0') break;
        }
        if (match) return entries[i].pid;
    }
    return (uint64_t)-1;
}

static void vid_setscale(uint64_t pid, int scale) {
    uint64_t msg[8] = {0};
    msg[0] = VID_SETSCALE;
    *(uint32_t*)((uint8_t*)msg + 8) = (uint32_t)scale;
    mod_send(pid, msg);
}

static void vid_clear(uint64_t pid) {
    uint64_t msg[8] = {0};
    msg[0] = VID_CLEAR;
    mod_send(pid, msg);
}

static void vid_putstr(uint64_t pid, const char* s, uint32_t color, int x, int y) {
    uint64_t msg[8] = {0};
    msg[0] = VID_PUTSTR;
    *(uint32_t*)((uint8_t*)msg + 8) = color;
    *(int32_t*)((uint8_t*)msg + 12) = x;
    *(int32_t*)((uint8_t*)msg + 16) = y;
    int i;
    for (i = 0; i < 43 && s[i]; i++)
        ((char*)msg)[20 + i] = s[i];
    ((char*)msg)[20 + i] = '\0';
    mod_send(pid, msg);
}

static void vid_putchar(uint64_t pid, char c, uint32_t color, int x, int y) {
    uint64_t msg[8] = {0};
    msg[0] = VID_PUTCHAR;
    ((char*)msg)[8] = c;
    *(uint32_t*)((uint8_t*)msg + 12) = color;
    *(int32_t*)((uint8_t*)msg + 16) = x;
    *(int32_t*)((uint8_t*)msg + 20) = y;
    mod_send(pid, msg);
}

static int kbd_getchar(uint64_t pid) {
    uint64_t msg[8] = {0};
    msg[0] = KBD_GETCHAR;
    mod_send(pid, msg);
    uint64_t reply[8];
    unsigned long long sender;
    do {
        sender = mod_recv_from(pid, reply);
    } while (sender != pid);
    if (reply[0] == 0)
        return (int)reply[1];
    return -1;
}

#define EXECV 2

static void elf_start_proc(const char str[128], uint64_t pid) {
    uint64_t msg[8];
    msg[0] = EXECV;

    int mi = 0;
    int pos = 0;
    const char* prefix = "C:\\NEODOS\\BIN\\";
    while (prefix[mi] && pos < 55) { ((char*)msg)[8 + pos] = prefix[mi]; mi++; pos++; }

    mi = 0;
    while (str[mi] && pos < 55) { ((char*)msg)[8 + pos] = str[mi]; mi++; pos++; }
    ((char*)msg)[8 + pos] = '\0';

    mod_send(pid, msg);
}

__attribute__((section(".text.start")))
void _start(void) {
    debug_puts("SHELL: starting\n");

    uint64_t vid = find_mod("video");
    uint64_t kbd = find_mod("kbd");
    uint64_t elf = find_mod("elf");

    if (vid == (uint64_t)-1 || kbd == (uint64_t)-1 || elf == (uint64_t)-1) {
        debug_puts("SHELL: missing video/kbd modules\n");
        while (1) asm volatile("pause");
    }

    debug_puts("SHELL: vid pid=");
    debug_putu(vid);
    debug_puts(" kbd pid=");
    debug_putu(kbd);
    debug_puts("\n");

    vid_setscale(vid, TEXT_SCALE);
    vid_clear(vid);
    vid_putstr(vid, "NeoDOS", COL_WHITE, 0, 0);
    vid_putstr(vid, "Type 'help' for commands", COL_WHITE, 0, FONT_H * TEXT_SCALE * 2);

    int row = 4;

    while (1) {
        vid_putstr(vid, PROMPT, COL_WHITE, 0, row * FONT_H * TEXT_SCALE);

        int col = 0;
        char input[128] = {0};

        while (1) {
            int c = kbd_getchar(kbd);
            if (c == -1) {
                asm volatile("pause");
                continue;
            }

            if (c == '\n' || c == '\r') {
                row++;
                elf_start_proc(input, elf);

                break;
            } else if (c == 8) {
                if (col > 0) {
                    col--;
                    int px = (PROMPT_LEN + col) * FONT_W * TEXT_SCALE;
                    vid_putchar(vid, ' ', COL_WHITE, px, row * FONT_H * TEXT_SCALE);
                }
            } else if (c >= 32 && col < LINE_MAX - 1) {
                int px = (PROMPT_LEN + col) * FONT_W * TEXT_SCALE;
                vid_putchar(vid, c, COL_WHITE, px, row * FONT_H * TEXT_SCALE);

                input[col] = c;
                col++;
            }
        }
    }
}
