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
    _g_term.scale = 2;

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

void vprintkf(const char* fmt, va_list args) {
    for(; *fmt; fmt++) {
        if(*fmt != '%') {
            putkc(*fmt);
            continue;
        }

        fmt++;

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
                putks(s);
                break;
            }
            case 'd':
            case 'i': {
                int v = va_arg(args, int);
                print_int(v);
                break;
            }
            case 'u': {
                unsigned int v = va_arg(args, unsigned int);
                print_uint(v, 10);
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
                print_uint(v, 16);
                break;
            }
            case 'p': {
                unsigned long v = (unsigned long)va_arg(args, void*);
                putks("0x");
                print_uint(v, 16);
                break;
            }
            case 'l':
                fmt++;
                if(*fmt == 'd') {
                    long v = va_arg(args, long);
                    print_int(v);
                } else if(*fmt == 'u') {
                    unsigned long v = va_arg(args, unsigned long);
                    print_uint(v, 10);
                } else if(*fmt == 'x' || *fmt == 'X') {
                    unsigned long v = va_arg(args, unsigned long);
                    print_uint(v, 16);
                } else if(*fmt == 'l') {
                    fmt++;
                    if(*fmt == 'd') {
                        long long v = va_arg(args, long long);
                        print_int(v);
                    } else if(*fmt == 'u') {
                        unsigned long long v = va_arg(args, unsigned long long);
                        print_uint(v, 10);
                    } else if(*fmt == 'x' || *fmt == 'X') {
                        unsigned long long v = va_arg(args, unsigned long long);
                        print_uint(v, 16);
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

void panic(const char* fmt, ...) {
    // memset(_g_term.fb->address, 0, _g_term.fb->height * _g_term.fb->pitch);

    // _g_term.x = 0;
    // _g_term.y = 1;

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
    // memset(_g_term.fb->address, 0, _g_term.fb->height * _g_term.fb->pitch);
    // _g_term.x = 0;
    // _g_term.y = 1;

    terminal_set_fg(0xff0000);
    putks("\n-------------------------------------\n");
    putks("        KERNEL PANIC\n");
    putks("-------------------------------------\n\n");
    terminal_set_fg(0xffffff);

    printkf("%s\n\n", msg);

    printkf("RIP: 0x%llx\nRSP: 0x%llx\n", frame->rip, frame->rsp);
    printkf("CS:  0x%llx\nSS: 0x%llx\n", frame->cs, frame->ss);
    printkf("RFLAGS: 0x%llx\n", frame->rflags);
    printkf("Error Code: 0x%llx\n", error_code);

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
