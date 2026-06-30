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

static int cursor_x = 0;
static int cursor_y = 0;
static int font_w = 0;
static int font_h = 0;
static int font_scale = 1;

static inline uint32_t make_color(uint8_t r, uint8_t g, uint8_t b) {
    if (fb_format == 1)
        return ((uint32_t)b << 16) | ((uint32_t)g << 8) | r;
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

static inline void putpixel(int x, int y, uint32_t color) {
    if (x < 0 || (uint32_t)x >= fb_width || y < 0 || (uint32_t)y >= fb_height) return;
    uint32_t* px = (uint32_t*)fb_addr + (uint64_t)y * fb_scanline + x;
    *px = color;
}

static FontGlyph* find_glyph(char c) {
    for (uint32_t i = 0; i < font->glyphCount; i++) {
        if (font->glyphs[i].ascii == (uint8_t)c)
            return &font->glyphs[i];
    }
    for (uint32_t i = 0; i < font->glyphCount; i++) {
        if (font->glyphs[i].ascii == (uint8_t)'?')
            return &font->glyphs[i];
    }
    return &font->glyphs[0];
}

static void draw_char(char c, int x, int y, uint32_t fg, uint32_t bg) {
    if (!font) return;
    FontGlyph* glyph = find_glyph(c);
    if (!glyph) return;

    uint8_t* bmp = (uint8_t*)glyph->bitmap;
    if (!bmp) return;

    uint32_t bytes_per_row = font->bytesPerGlyph / font_h;

    for (int row = 0; row < font_h; row++) {
        for (int col = 0; col < font_w; col++) {
            uint32_t byte_idx = row * bytes_per_row + (col / 8);
            uint8_t bit_mask = 0x80 >> (col % 8);
            uint32_t color = (bmp[byte_idx] & bit_mask) ? fg : bg;

            for (int dy = 0; dy < font_scale; dy++)
                for (int dx = 0; dx < font_scale; dx++)
                    putpixel(x + col * font_scale + dx, y + row * font_scale + dy, color);
        }
    }
}

static void draw_string(const char* s, int x, int y, uint32_t fg, uint32_t bg) {
    while (*s) {
        if (*s == '\n') { x = 0; y += font_h * font_scale; }
        else { draw_char(*s, x, y, fg, bg); x += font_w * font_scale; }
        s++;
    }
}

static uint32_t term_fg;
static uint32_t term_bg;

static void term_scroll(void) {
    uint32_t* fb = (uint32_t*)fb_addr;
    int row_h = font_h * font_scale;
    int row_px = row_h * fb_scanline;
    int fb_px = fb_height * fb_scanline;

    for (int i = 0; i < fb_px - row_px; i++)
        fb[i] = fb[i + row_px];
    for (int i = fb_px - row_px; i < fb_px; i++)
        fb[i] = term_bg;

    cursor_y = fb_height - row_h;
}

static void term_putchar(char c) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y += font_h * font_scale;
    } else if (c == '\r') {
        cursor_x = 0;
    } else {
        draw_char(c, cursor_x, cursor_y, term_fg, term_bg);
        cursor_x += font_w * font_scale;
    }

    if (cursor_x + font_w * font_scale > (int)fb_width) { cursor_x = 0; cursor_y += font_h * font_scale; }
    if (cursor_y + font_h * font_scale > (int)fb_height) term_scroll();
}

static void term_puts(const char* s) {
    while (*s) term_putchar(*s++);
}

static void clear_screen(uint32_t color) {
    uint32_t* fb = (uint32_t*)fb_addr;
    for (int y = 0; y < (int)fb_height; y++)
        for (int x = 0; x < (int)fb_width; x++)
            fb[y * fb_scanline + x] = color;
    cursor_x = 0;
    cursor_y = 0;
}

static void handle_ipc(unsigned char* buf, unsigned long long sender) {
    (void)sender;
    uint64_t cmd = *(uint64_t*)buf;
    if (cmd == 1) {
        term_puts((const char*)(buf + 8));
    } else if (cmd == 2) {
        clear_screen(make_color(0x00, 0x10, 0x30));
        draw_string("NeoDOS Video Console", 0, 0,
                    make_color(0xFF, 0xFF, 0x00), make_color(0x00, 0x10, 0x30));
        cursor_y = font_h * font_scale;
    }
}

__attribute__((section(".text.start")))
void _start(void) {
    FbInfo fb_info;
    unsigned long long ret = mod_syscall(SYSCALL_MOD, MOD_BOOTINFO,
        BOOTINFO_ARG(BOOTINFO_FB, sizeof(fb_info)), (unsigned long long)&fb_info);
    if (ret != sizeof(fb_info) || !fb_info.addr) {
        debug_puts("VIDEO: no framebuffer available\n");
        while (1) asm volatile("pause");
    }

    uint64_t fb_pages = (fb_info.size + 0xFFF) / 0x1000;
    uint64_t fb_vaddr = mod_syscall(SYSCALL_MOD, MOD_PHYS_MAP, fb_info.addr, fb_pages);
    if (fb_vaddr == (uint64_t)-1) {
        debug_puts("VIDEO: failed to map framebuffer\n");
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
        debug_puts("VIDEO: no font available\n");
        while (1) asm volatile("pause");
    }

    uint64_t font_size = sizeof(FontInfo) +
        font_info.glyph_count * sizeof(FontGlyph) +
        font_info.glyph_count * font_info.bytes_per_glyph;
    uint64_t font_pages = (font_size + 0xFFF) / 0x1000;
    uint64_t font_vaddr = mod_syscall(SYSCALL_MOD, MOD_PHYS_MAP, font_info.addr, font_pages);
    if (font_vaddr == (uint64_t)-1) {
        debug_puts("VIDEO: failed to map font\n");
        while (1) asm volatile("pause");
    }

    font = (FontInfo*)font_vaddr;
    font_w = font->fontWidth;
    font_h = font->fontHeight;
    font_scale = 1;

    mod_register("video");

    term_fg = make_color(0xC0, 0xC0, 0xC0);
    term_bg = make_color(0x00, 0x00, 0x40);
    clear_screen(term_bg);

    draw_string("NeoDOS Video Module", 0, 0,
                make_color(0xFF, 0xFF, 0x00), term_bg);
    cursor_y = font_h * font_scale;

    debug_puts("VIDEO: initialized (fb=");
    debug_putu(fb_width); debug_puts("x"); debug_putu(fb_height);
    debug_puts(", font="); debug_putu(font_w); debug_puts("x"); debug_putu(font_h);
    debug_puts(")\n");

    term_puts("NeoDOS video module ready\n");

    while (1) {
        unsigned char msg[64];
        unsigned long long sender = mod_recv(msg);
        if (sender != (unsigned long long)-1) {
            handle_ipc(msg, sender);
        } else {
            asm volatile("pause");
        }
    }
}
