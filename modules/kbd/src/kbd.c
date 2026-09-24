#include "modstd.h"
#include <stdint.h>

REGISTER_MODULE("kbd");

#define KBD_GETCHAR  1

#define PORT_W(port, width, dir) \
    ((uint64_t)((port) & 0xFFFF) | (((uint64_t)(width) & 0xFF) << 16) | (((uint64_t)(dir) & 0xFF) << 24))

#define DATA   0x60
#define STATUS 0x64
#define CMD    0x64
#define OBF    0x01
#define IBF    0x02

#define KBD_ACK  0xFA
#define KBD_BAT  0xAA

#define CFG_DISABLE_KBD_CLK 0x10
#define CFG_DISABLE_MOUSE_CLK 0x20
#define CFG_ENABLE_TRANSLATE 0x40

#define BUFLEN 128

static uint8_t buf[BUFLEN];
static volatile int head = 0, tail = 0;

static uint8_t inb(uint16_t p) {
    return (uint8_t)mod_syscall4(SYSCALL_MOD, MOD_PORT_IO, PORT_W(p, 1, PORT_IN), 0, 0);
}

static void outb(uint16_t p, uint8_t v) {
    mod_syscall4(SYSCALL_MOD, MOD_PORT_IO, PORT_W(p, 1, PORT_OUT), v, 0);
}

static const unsigned char s2a[128] = {
    0,27,'1','2','3','4','5','6','7','8','9','0','-','=',8,
    9,'q','w','e','r','t','y','u','i','o','p','[',']',10,
    0,'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,'\\','z','x','c','v','b','n','m',',','.','/',0,
    '*',0,' ',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    '7','8','9','-','4','5','6','+','1','2','3','0','.',
    0,0,0,0,0
};

static void poll(void) {
    uint8_t st = inb(STATUS);
    if (!(st & OBF)) return;
    uint8_t sc = inb(DATA);
    if (sc < 0x80) {
        unsigned char c = s2a[sc];
        if (c) {
            int n = (head + 1) % BUFLEN;
            if (n != tail) { buf[head] = c; head = n; }
        }
    }
}

static void handle_ipc(unsigned char* msg, unsigned long long sender) {
    uint64_t* a = (uint64_t*)msg;
    uint64_t reply[8] = {0};

    switch (a[0]) {
    case KBD_GETCHAR: {
        poll();
        if (head != tail) {
            reply[0] = 0;
            reply[1] = buf[tail];
            tail = (tail + 1) % BUFLEN;
        } else {
            reply[0] = -1;
        }
        break;
    }
    default:
        reply[0] = -1;
    }

    mod_send(sender, reply);
}

static int kbd_wait_input(void) {
    for (int i = 0; i < 10000; i++) {
        if (inb(STATUS) & OBF) return 0;
        for (volatile int j = 0; j < 100; j++);
    }
    return -1;
}

static int kbd_wait_output(void) {
    for (int i = 0; i < 10000; i++) {
        if (!(inb(STATUS) & IBF)) return 0;
        for (volatile int j = 0; j < 100; j++);
    }
    return -1;
}

static void kbd_send_cmd(uint8_t cmd) {
    kbd_wait_output();
    outb(DATA, cmd);
}

static uint8_t kbd_read_data(void) {
    kbd_wait_input();
    return inb(DATA);
}

static void drain(void) {
    for (int i = 0; i < 1000; i++) {
        if (!(inb(STATUS) & OBF)) break;
        inb(DATA);
        for (volatile int j = 0; j < 100; j++);
    }
}

static int read_config(void) {
    if (kbd_wait_output()) return -1;
    outb(CMD, 0x20);
    if (kbd_wait_input()) return -1;
    return inb(DATA);
}

static int write_config(uint8_t cfg) {
    if (kbd_wait_output()) return -1;
    outb(CMD, 0x60);
    if (kbd_wait_output()) return -1;
    outb(DATA, cfg);
    return 0;
}

static int ps2_command(uint8_t cmd) {
    if (kbd_wait_output()) return -1;
    outb(CMD, cmd);
    return 0;
}

void init(void) {
    debug_puts("KBD: init\n");

    ps2_command(0xAD);
    ps2_command(0xA7);
    drain();

    int cfg = read_config();
    if (cfg < 0) cfg = 0;
    uint8_t new_cfg = (uint8_t)cfg;
    new_cfg &= ~CFG_DISABLE_KBD_CLK;
    new_cfg |= CFG_ENABLE_TRANSLATE;
    new_cfg |= CFG_DISABLE_MOUSE_CLK;
    write_config(new_cfg);

    ps2_command(0xAE);
    drain();

    kbd_send_cmd(0xFF);
    uint8_t ack = kbd_read_data();
    uint8_t bat = kbd_read_data();

    if (ack == KBD_ACK && bat == KBD_BAT) {
        kbd_send_cmd(0xF6);
        kbd_read_data();
        kbd_send_cmd(0xF4);
        ack = kbd_read_data();
        if (ack == KBD_ACK)
            debug_puts("KBD: ready\n");
        else
            debug_puts("KBD: scan failed\n");
    } else {
        debug_puts("KBD: reset failed\n");
    }
}

void loop(void) {
    unsigned char msg[64];
    unsigned long long snd = mod_recv(msg);
    if (snd == (unsigned long long)-1) {
        poll();
        asm volatile("pause");
    } else {
        handle_ipc(msg, snd);
    }
}
