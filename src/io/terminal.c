#include "terminal.h"

#include <stdarg.h>

#include "../font/glyphs.h"
#include "../std/string.h"
#include "../sync/spinlock.h"

terminal_t _g_term;

static spinlock_t terminal_lock = {0};

void terminal_init(struct limine_framebuffer* fb) {
    _g_term = (terminal_t){0};

    _g_term.fg = 0xffffff;
    _g_term.bg = 0x000000;
    _g_term.x = 0;
    _g_term.y = 1;
    _g_term.font = &glyphs[0][0];
    _g_term.scale = 1;

    _g_term.fb = fb;
    _g_term.max_x = fb->width / (FONT_GLYPH_WIDTH * _g_term.scale);
    _g_term.max_y = fb->height / (FONT_GLYPH_HEIGHT * _g_term.scale);
}

static void draw_pixel(terminal_t* term, size_t x, size_t y, uint32_t color) {
    if(x >= term->fb->width || y >= term->fb->height) {
        return;
    }

    uint8_t *fb_addr = term->fb->address;
    size_t offset = (y * term->fb->pitch) + (x * 4);

    *(uint32_t *)(fb_addr + offset) = color;
}

static void draw_char_at(terminal_t* term, char c, size_t x, size_t y, uint32_t color) {
    char *glyph = &term->font[c * FONT_GLYPH_HEIGHT];
    size_t start_x = x * FONT_GLYPH_WIDTH * term->scale;
    size_t start_y = y * FONT_GLYPH_HEIGHT * term->scale;

    for(size_t cy = 0; cy < FONT_GLYPH_HEIGHT; cy++) {
        uint8_t row_byte = glyph[cy];
        for(size_t cx = 0; cx < FONT_GLYPH_WIDTH; cx++) {
            if(row_byte & (0x80 >> cx)) {
                size_t pix_x_offs = FONT_GLYPH_WIDTH - 1 - cx;
                for(size_t sy = 0; sy < term->scale; sy++) {
                    for(size_t sx = 0; sx < term->scale; sx++) {
                        size_t target_x = start_x + (pix_x_offs * term->scale) + sx;
                        size_t target_y = start_y + (cy * term->scale) + sy;
                        draw_pixel(term, target_x, target_y, color);
                    }
                }
            }
        }
    }
}

static void clear_char_at(terminal_t* term, size_t x, size_t y) {
    size_t start_x = x * FONT_GLYPH_WIDTH * term->scale;
    size_t start_y = y * FONT_GLYPH_HEIGHT * term->scale;

    for(size_t cy = 0; cy < FONT_GLYPH_HEIGHT * term->scale; cy++) {
        for(size_t cx = 0; cx < FONT_GLYPH_WIDTH * term->scale; cx++) {
            draw_pixel(term, start_x + cx, start_y + cy, term->bg);
        }
    }
}

void scroll_terminal(terminal_t* term) {
    size_t line_height = FONT_GLYPH_HEIGHT * term->scale;
    size_t scroll_bytes = line_height * term->fb->pitch;
    size_t move_bytes = (term->fb->height * term->fb->pitch) - scroll_bytes;
    memmove(term->fb->address, term->fb->address + scroll_bytes, move_bytes);
    memset(term->fb->address + move_bytes, 0, scroll_bytes);
}

void terminal_set_fg(uint32_t color) {
    _g_term.fg = color;
}

void terminal_set_bg(uint32_t color) {
    _g_term.bg = color;
}

int putkc(char c) {
    if(c == '\n') {
        _g_term.y++;
        _g_term.x = 0;
        if(_g_term.y >= _g_term.max_y) {
            scroll_terminal(&_g_term);
            _g_term.y = _g_term.max_y - 1;
        }
        return 0;
    }

    if(c == '\b') {
        if(_g_term.x > 0) {
            _g_term.x--;
            clear_char_at(&_g_term, _g_term.x, _g_term.y);
        }
        return 0;
    }

    if(c == '\r') {
        _g_term.x = 0;
        return 0;
    }

    if(c < 0x20) {
        return -1;
    }

    if(_g_term.x >= _g_term.max_x) {
        _g_term.x = 0;
        _g_term.y++;
        if(_g_term.y >= _g_term.max_y) {
            scroll_terminal(&_g_term);
            _g_term.y = _g_term.max_y - 1;
        }
    }

    draw_char_at(&_g_term, c, _g_term.x, _g_term.y, _g_term.fg);
    _g_term.x++;

    return 0;
}

int putks(const char* s) {
    int ret = 0;
    while(*s) {
        ret |= putkc(*s++);
    }
    return ret;
}

static void print_uint(unsigned long long value, int base) {
    char buffer[32];
    const char* digits = "0123456789abcdef";

    int i = 0;
    if(value == 0) {
        putkc('0');
        return;
    }

    while(value > 0) {
        buffer[i++] = digits[value % base];
        value /= base;
    }

    while(i--) {
        putkc(buffer[i]);
    }
}

static void print_int(long long value) {
    if(value < 0) {
        putkc('-');
        value = -value;
    }
    print_uint((unsigned long long)value, 10);
}
static int count_digits(unsigned long long v, int base) {
    if(v == 0) return 1;
    int count = 0;
    while(v) {
        v /= base;
        count++;
    }
    return count;
}

void print_uint_padded(unsigned long long v, int base, int width, char pad_char) {
    int digits = count_digits(v, base);
    for(int i = digits; i < width; i++) {
        putkc(pad_char);
    }
    print_uint(v, base);
}

void print_int_padded(long long v, int width, char pad_char) {
    int is_negative = 0;
    unsigned long long uv;

    if(v < 0) {
        is_negative = 1;
        uv = -v;
    } else {
        uv = v;
    }

    int digits = count_digits(uv, 10);
    int total = digits + (is_negative ? 1 : 0);

    if(pad_char == '0' && is_negative) {
        putkc('-');
        for(int i = digits; i < width - 1; i++) {
            putkc('0');
        }
        print_uint(uv, 10);
    } else {
        for(int i = total; i < width; i++) {
            putkc(pad_char);
        }
        if(is_negative) putkc('-');
        print_uint(uv, 10);
    }
}

void vprintkf(const char* fmt, va_list args) {
    for(; *fmt; fmt++) {
        if(*fmt != '%') {
            putkc(*fmt);
            continue;
        }
        fmt++;

        char pad_char = ' ';
        if(*fmt == '0') {
            pad_char = '0';
            fmt++;
        }

        int width = 0;
        while(*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }

        switch(*fmt) {
            case '%':
                putkc('%');
                break;
            case 'c': {
                char c = (char)va_arg(args, int);
                putkc(c);
                break;
            }
            case 's': {
                const char *s = va_arg(args, const char*);
                int len = 0;
                for(const char *p = s; *p; p++) len++;
                for(int i = len; i < width; i++) putkc(pad_char);
                putks(s);
                break;
            }
            case 'd':
            case 'i': {
                int v = va_arg(args, int);
                print_int_padded(v, width, pad_char);
                break;
            }
            case 'u': {
                unsigned int v = va_arg(args, unsigned int);
                print_uint_padded(v, 10, width, pad_char);
                break;
            }
            case 'k': {
                unsigned int v = va_arg(args, unsigned int);
                terminal_set_fg(v);
                break;
            }
            case 'r':
                terminal_set_fg(0xffffff);
                break;
            case 'x':
            case 'X': {
                unsigned int v = va_arg(args, unsigned int);
                print_uint_padded(v, 16, width, pad_char);
                break;
            }
            case 'p': {
                unsigned long v = (unsigned long)va_arg(args, void*);
                putks("0x");
                print_uint_padded(v, 16, width ? width : 16, '0');
                break;
            }
            case 'l':
                fmt++;
                if(*fmt == 'd') {
                    long v = va_arg(args, long);
                    print_int_padded(v, width, pad_char);
                } else if(*fmt == 'u') {
                    unsigned long v = va_arg(args, unsigned long);
                    print_uint_padded(v, 10, width, pad_char);
                } else if(*fmt == 'x' || *fmt == 'X') {
                    unsigned long v = va_arg(args, unsigned long);
                    print_uint_padded(v, 16, width, pad_char);
                } else if(*fmt == 'l') {
                    fmt++;
                    if(*fmt == 'd') {
                        long long v = va_arg(args, long long);
                        print_int_padded(v, width, pad_char);
                    } else if(*fmt == 'u') {
                        unsigned long long v = va_arg(args, unsigned long long);
                        print_uint_padded(v, 10, width, pad_char);
                    } else if(*fmt == 'x' || *fmt == 'X') {
                        unsigned long long v = va_arg(args, unsigned long long);
                        print_uint_padded(v, 16, width, pad_char);
                    }
                }
                break;
            default:
                putkc('%');
                putkc(*fmt);
        }
    }
}

void printkf(const char* fmt, ...) {
    uint64_t flags = spin_lock(&terminal_lock);
    va_list args;
    va_start(args, fmt);
    vprintkf(fmt, args);
    va_end(args);
    spin_unlock(&terminal_lock, flags);
}

void terminal_clear() {
    memset(_g_term.fb->address, 0, _g_term.fb->height * _g_term.fb->pitch);

    _g_term.x = 0;
    _g_term.y = 1;
}

void panic(const char* fmt, ...) {
    terminal_set_fg(0xff0000);
    putks("\n-------------------------------------\n");
    putks("        KERNEL PANIC\n");
    putks("-------------------------------------\n\n");
    terminal_set_fg(0xffffff);

    va_list args;
    va_start(args, fmt);
    vprintkf(fmt, args);
    va_end(args);

    for(;;) {
        asm("hlt");
    }
}

void panic_with_frame(struct interrupt_frame* frame, uint64_t error_code, const char* msg) {
    terminal_set_fg(0xff0000);
    putks("\n-------------------------------------\n");
    putks("        KERNEL PANIC\n");
    putks("-------------------------------------\n\n");
    terminal_set_fg(0xffffff);

    printkf("REASON: %s\n\n", msg);

    printkf("ERR: 0x%llx\n", error_code);
    if (error_code != 0) {
        printkf("  PRESENT: %s\n", (error_code & 0x1) ? "Y" : "N");
        printkf("  WRITE: %s\n", (error_code & 0x2) ? "Y" : "READ");
        printkf("  USER: %s\n", (error_code & 0x4) ? "Y" : "SUPER");
        printkf("  RESERVED WRITE: %s\n", (error_code & 0x8) ? "Y" : "N");
        printkf("  INSTRUCTION FETCH: %s\n", (error_code & 0x10) ? "Y" : "N");
    }
    putks("\n");

    terminal_set_fg(0x00ffff);
    putks("CPU:\n");
    terminal_set_fg(0xffffff);
    printkf("  RIP: 0x%016llx\n", frame->rip);
    printkf("  RSP: 0x%016llx\n", frame->rsp);
    printkf("  CS:  0x%04llx\n", frame->cs);
    printkf("  SS:  0x%04llx\n", frame->ss);
    putks("\n");

    printkf("RFLAGS: 0x%016llx\n", frame->rflags);
    printkf("  CF=%d PF=%d AF=%d ZF=%d SF=%d TF=%d IF=%d DF=%d OF=%d\n",
            (frame->rflags & (1<<0)) ? 1 : 0,
            (frame->rflags & (1<<2)) ? 1 : 0,
            (frame->rflags & (1<<4)) ? 1 : 0,
            (frame->rflags & (1<<6)) ? 1 : 0,
            (frame->rflags & (1<<7)) ? 1 : 0,
            (frame->rflags & (1<<8)) ? 1 : 0,
            (frame->rflags & (1<<9)) ? 1 : 0,
            (frame->rflags & (1<<10)) ? 1 : 0,
            (frame->rflags & (1<<11)) ? 1 : 0);
    printkf("  IOPL=%d NT=%d RF=%d VM=%d\n",
            (frame->rflags >> 12) & 3,
            (frame->rflags & (1<<14)) ? 1 : 0,
            (frame->rflags & (1<<16)) ? 1 : 0,
            (frame->rflags & (1<<17)) ? 1 : 0);
    putks("\n");

    uint64_t cr0, cr2, cr3, cr4;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    asm volatile("mov %%cr2, %0" : "=r"(cr2));
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    asm volatile("mov %%cr4, %0" : "=r"(cr4));

    terminal_set_fg(0xffff00);
    putks("CONTROL REGISTERS:\n");
    terminal_set_fg(0xffffff);
    printkf("  CR0: 0x%016llx\n", cr0);
    printkf("  CR2: 0x%016llx\n", cr2);
    printkf("  CR3: 0x%016llx\n", cr3);
    printkf("  CR4: 0x%016llx\n", cr4);
    putks("\n");

    terminal_set_fg(0xff00ff);
    putks("TRACE:\n");
    terminal_set_fg(0xffffff);
    uint64_t* stack = (uint64_t*)frame->rsp;
    for (int i = 0; i < 8; i++) {
        printkf("  [RSP+0x%02x]: 0x%016llx\n", i * 8, stack[i]);
    }
    putks("\n");

    terminal_set_fg(0xff0000);
    putks("SYSTEM HALTED");
    terminal_set_fg(0xffffff);

    for(;;) asm("hlt");
}

void printkf_info(const char* fmt, ...) {
    uint64_t flags = spin_lock(&terminal_lock);

    terminal_set_fg(0x00aaff);
    putks("[INFO] ");
    terminal_set_fg(0xffffff);

    va_list args;
    va_start(args, fmt);
    vprintkf(fmt, args);
    va_end(args);

    spin_unlock(&terminal_lock, flags);
}

void printkf_ok(const char* fmt, ...) {
    uint64_t flags = spin_lock(&terminal_lock);

    terminal_set_fg(0xaaffaa);
    putks("[ OK ] ");
    terminal_set_fg(0xffffff);

    va_list args;
    va_start(args, fmt);
    vprintkf(fmt, args);
    va_end(args);

    spin_unlock(&terminal_lock, flags);
}

void printkf_warn(const char* fmt, ...) {
    uint64_t flags = spin_lock(&terminal_lock);

    terminal_set_fg(0xffaa00);
    putks("[WARN] ");
    terminal_set_fg(0xffffff);

    va_list args;
    va_start(args, fmt);
    vprintkf(fmt, args);
    va_end(args);

    spin_unlock(&terminal_lock, flags);
}

void printkf_error(const char* fmt, ...) {

    uint64_t flags = spin_lock(&terminal_lock);
    terminal_set_fg(0xff0000);
    putks("[ERR ] ");
    terminal_set_fg(0xffffff);

    va_list args;
    va_start(args, fmt);
    vprintkf(fmt, args);
    va_end(args);

    spin_unlock(&terminal_lock, flags);
}
