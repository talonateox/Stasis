#pragma once

#include <stddef.h>

#include "../interrupts/interrupts.h"
#include "../limine.h"

typedef struct {
    size_t x, y;
    size_t max_x, max_y;
    char *font;
    uint32_t fg;
    uint32_t bg;
    struct limine_framebuffer *fb;

    size_t scale;
} terminal_t;

void terminal_init(struct limine_framebuffer *fb);

void terminal_set_fg(uint32_t color);
void terminal_set_bg(uint32_t color);
void terminal_clear();

int putkc(char c);
int putks(const char *s);

void panic(const char *fmt, ...);
void printkf(const char *fmt, ...);
void printkf_info(const char *fmt, ...);
void printkf_ok(const char *fmt, ...);
void printkf_warn(const char *fmt, ...);
void printkf_error(const char *fmt, ...);

void panic_with_frame(struct interrupt_frame *frame, uint64_t error_code, const char *msg);
