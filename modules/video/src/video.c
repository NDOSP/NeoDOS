#include "modlib.h"
#include <video.h>
#include <stdint.h>
#include <stddef.h>

static volatile uint8_t* fb_addr = 0;
static uint32_t fb_width = 0;
static uint32_t fb_height = 0;
static uint32_t fb_scanline = 0;
static int fb_format = 0;

static FontInfo* font = 0;
static int font_w = 0;
static int font_h = 0;
static int text_scale = 0;

MODINFO("video");

static inline uint32_t pack_color(uint8_t r, uint8_t g, uint8_t b) {
    if (fb_format == 1)
        return ((uint32_t)b << 16) | ((uint32_t)g << 8) | r;
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

static inline void putpixel(int x, int y, uint32_t color) {
    if (x < 0 || (uint32_t)x >= fb_width || y < 0 || (uint32_t)y >= fb_height) return;
    ((uint32_t*)fb_addr)[(uint64_t)y * fb_scanline + x] = color;
}

static FontGlyph* find_glyph(char c) {
    for (uint32_t i = 0; i < font->glyphCount; i++)
        if (font->glyphs[i].ascii == (uint8_t)c)
            return &font->glyphs[i];
    for (uint32_t i = 0; i < font->glyphCount; i++)
        if (font->glyphs[i].ascii == '?')
            return &font->glyphs[i];
    return &font->glyphs[0];
}

static void draw_char(char c, int x, int y, uint32_t color) {
    if (!font) return;
    FontGlyph* glyph = find_glyph(c);
    if (!glyph) return;
    uint8_t* bmp = (uint8_t*)glyph->bitmap;
    if (!bmp) return;

    uint32_t bytes_per_row = font->bytesPerGlyph / font_h;
    int s = text_scale;
    for (int row = 0; row < font_h; row++) {
        for (int col = 0; col < font_w; col++) {
            uint32_t byte_idx = row * bytes_per_row + (col / 8);
            if (bmp[byte_idx] & (0x80 >> (col % 8))) {
                for (int dy = 0; dy < s; dy++)
                    for (int dx = 0; dx < s; dx++)
                        putpixel(x + col * s + dx, y + row * s + dy, color);
            } else {
                for (int dy = 0; dy < s; dy++)
                    for (int dx = 0; dx < s; dx++)
                        putpixel(x + col * s + dx, y + row * s + dy, 0x00000000);
            }
        }
    }
}

static void draw_string(const char* str, int x, int y, uint32_t color) {
    if (!str) return;
    int s = text_scale;
    while (*str) {
        if (*str == '\n') { x = 0; y += font_h * s; }
        else { draw_char(*str, x, y, color); x += font_w * s; }
        str++;
    }
}

static void clear_screen(void) {
    uint32_t black = pack_color(0, 0, 0);
    for (uint32_t y = 0; y < fb_height; y++)
        for (uint32_t x = 0; x < fb_width; x++)
            ((uint32_t*)fb_addr)[(uint64_t)y * fb_scanline + x] = black;
}

static void fill_rect(int x, int y, int w, int h, uint32_t color) {
    for (int row = 0; row < h; row++)
        for (int col = 0; col < w; col++)
            putpixel(x + col, y + row, color);
}

static void handle_ipc(unsigned char* buf, unsigned long long sender) {
    (void)sender;
    uint64_t cmd = *(uint64_t*)buf;

    switch (cmd) {
    case 1: { // putChar(char, color, x, y)
        char c = (char)buf[8];
        uint32_t color = *(uint32_t*)(buf + 12);
        int32_t x = *(int32_t*)(buf + 16);
        int32_t y = *(int32_t*)(buf + 20);
        draw_char(c, x, y, color);
        break;
    }
    case 2: { // putString(string, color, x, y)
        uint32_t color = *(uint32_t*)(buf + 8);
        int32_t x = *(int32_t*)(buf + 12);
        int32_t y = *(int32_t*)(buf + 16);
        draw_string((const char*)(buf + 20), x, y, color);
        break;
    }
    case 3: { // drawRectangle(color, h, w, x, y)
        uint32_t color = *(uint32_t*)(buf + 8);
        int32_t h = *(int32_t*)(buf + 12);
        int32_t w = *(int32_t*)(buf + 16);
        int32_t x = *(int32_t*)(buf + 20);
        int32_t y = *(int32_t*)(buf + 24);
        fill_rect(x, y, w, h, color);
        break;
    }
    case 4: // clearScreen — pure black
        clear_screen();
        break;
    case 5: { // setTextScale(scale)
        uint32_t scale_val = *(uint32_t*)(buf + 8);
        if (scale_val >= 1 && scale_val <= 4)
            text_scale = (int)scale_val;
        break;
    }
    }
}

__attribute__((section(".text.start")))
void _start(void) {
    FbInfo fb_info;
    unsigned long long ret = mod_syscall(SYSCALL_MOD, MOD_BOOTINFO,
        BOOTINFO_ARG(BOOTINFO_FB, sizeof(fb_info)), (unsigned long long)&fb_info);
    if (ret != sizeof(fb_info) || !fb_info.addr) {
        debug_puts("VIDEO: no framebuffer\n");
        while (1) asm volatile("pause");
    }

    uint64_t fb_pages = (fb_info.size + 0xFFF) / 0x1000;
    uint64_t fb_vaddr = mod_syscall4(SYSCALL_MOD, MOD_PHYS_MAP, fb_info.addr, fb_pages, 0);
    if (fb_vaddr == (uint64_t)-1) {
        debug_puts("VIDEO: failed to map fb\n");
        while (1) asm volatile("pause");
    }

    fb_addr = (volatile uint8_t*)fb_vaddr;
    fb_width = fb_info.width;
    fb_height = fb_info.height;
    fb_scanline = fb_info.scanline;
    fb_format = fb_info.format;

    FontSysInfo font_info;
    ret = mod_syscall(SYSCALL_MOD, MOD_BOOTINFO,
        BOOTINFO_ARG(BOOTINFO_FONT, sizeof(font_info)), (unsigned long long)&font_info);
    if (ret != sizeof(font_info) || !font_info.addr) {
        debug_puts("VIDEO: no font\n");
        while (1) asm volatile("pause");
    }

    uint64_t font_size = sizeof(FontInfo) +
        font_info.glyph_count * sizeof(FontGlyph) +
        font_info.glyph_count * font_info.bytes_per_glyph;
    uint64_t font_pages = (font_size + 0xFFF) / 0x1000;
    uint64_t font_vaddr = mod_syscall4(SYSCALL_MOD, MOD_PHYS_MAP, font_info.addr, font_pages, 0);
    if (font_vaddr == (uint64_t)-1) {
        debug_puts("VIDEO: failed to map font\n");
        while (1) asm volatile("pause");
    }

    font = (FontInfo*)font_vaddr;
    font_w = font->fontWidth;
    font_h = font->fontHeight;

    uint64_t scale = 0;
    ret = mod_syscall(SYSCALL_MOD, MOD_BOOTINFO,
        BOOTINFO_ARG(BOOTINFO_FONT_SCALE, sizeof(scale)), (unsigned long long)&scale);
    if (ret == sizeof(scale) && scale >= 1 && scale <= 4)
        text_scale = (int)scale;
    else
        text_scale = 1;

    debug_puts("VIDEO: ready\n");

    while (1) {
        unsigned char msg[64];
        unsigned long long sender = mod_recv(msg);
        if (sender != (unsigned long long)-1)
            handle_ipc(msg, sender);
        else
            asm volatile("pause");
    }
}
