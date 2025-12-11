#include "string.h"

#include <stdarg.h>

void* memcpy(void* restrict dest, const void* restrict src, size_t n) {
    uint8_t* restrict pdest = (uint8_t* restrict)dest;
    const uint8_t* restrict psrc = (const uint8_t* restrict)src;

    for (size_t i = 0; i < n; i++) {
        pdest[i] = psrc[i];
    }

    return dest;
}

void* memset(void* s, int c, size_t n) {
    uint8_t* p = (uint8_t*)s;

    for (size_t i = 0; i < n; i++) {
        p[i] = (uint8_t)c;
    }

    return s;
}

void* memmove(void* dest, const void* src, size_t n) {
    uint8_t* pdest = (uint8_t*)dest;
    const uint8_t* psrc = (const uint8_t*)src;

    if (src > dest) {
        for (size_t i = 0; i < n; i++) {
            pdest[i] = psrc[i];
        }
    } else if (src < dest) {
        for (size_t i = n; i > 0; i--) {
            pdest[i - 1] = psrc[i - 1];
        }
    }

    return dest;
}

int memcmp(const void* s1, const void* s2, size_t n) {
    const uint8_t* p1 = (const uint8_t*)s1;
    const uint8_t* p2 = (const uint8_t*)s2;

    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] < p2[i] ? -1 : 1;
        }
    }

    return 0;
}

int strcmp(const char* s1, const char* s2) {
    const unsigned char* p1 = (const unsigned char*)s1;
    const unsigned char* p2 = (const unsigned char*)s2;

    while (*p1 != '\0' && *p1 == *p2) {
        p1++;
        p2++;
    }

    return (int)*p1 - (int)*p2;
}

void strcpy(char* s1, const char* s2) { while ((*s1++ = *s2++) != '\0'); }

size_t strlen(const char* str) {
    size_t length = 0;
    while (*str != '\0') {
        length++;
        str++;
    }
    return length;
}

int streq(const char* a, const char* b) {
    while (*a && *b) {
        if (*a != *b) return 0;
        a++;
        b++;
    }
    return *a == *b;
}

void strcat(char* dst, const char* src) {
    while (*dst) dst++;
    while (*src) {
        *dst++ = *src++;
    }
    *dst = '\0';
}

char* strncpy(char* dest, const char* src, size_t n) {
    size_t i;

    for (i = 0; i < n && src[i] != '\0'; i++) dest[i] = src[i];
    for (; i < n; i++) dest[i] = '\0';

    return dest;
}

int vsnprintf(char* buf, size_t size, const char* fmt, va_list args) {
    if (!buf || size == 0) return 0;

    size_t pos = 0;

    for (; *fmt && pos < size - 1; fmt++) {
        if (*fmt != '%') {
            buf[pos++] = *fmt;
            continue;
        }
        fmt++;

        char pad_char = ' ';
        if (*fmt == '0') {
            pad_char = '0';
            fmt++;
        }

        int width = 0;
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }

        switch (*fmt) {
            case '%':
                if (pos < size - 1) buf[pos++] = '%';
                break;
            case 'c': {
                char c = (char)va_arg(args, int);
                if (pos < size - 1) buf[pos++] = c;
                break;
            }
            case 's': {
                const char* s = va_arg(args, const char*);
                while (*s && pos < size - 1) {
                    buf[pos++] = *s++;
                }
                break;
            }
            case 'x':
            case 'X': {
                unsigned int v = va_arg(args, unsigned int);
                const char* digits = "0123456789abcdef";
                char temp[16];
                int i = 0;

                if (v == 0) {
                    temp[i++] = '0';
                } else {
                    while (v > 0 && i < 16) {
                        temp[i++] = digits[v % 16];
                        v /= 16;
                    }
                }

                while (i < width && pos < size - 1) {
                    buf[pos++] = pad_char;
                    width--;
                }

                while (i-- > 0 && pos < size - 1) {
                    buf[pos++] = temp[i];
                }
                break;
            }
            case 'd':
            case 'i': {
                int v = va_arg(args, int);
                if (v < 0 && pos < size - 1) {
                    buf[pos++] = '-';
                    v = -v;
                }

                char temp[16];
                int i = 0;
                if (v == 0) {
                    temp[i++] = '0';
                } else {
                    while (v > 0 && i < 16) {
                        temp[i++] = '0' + (v % 10);
                        v /= 10;
                    }
                }

                while (i-- > 0 && pos < size - 1) {
                    buf[pos++] = temp[i];
                }
                break;
            }
        }
    }

    buf[pos] = '\0';
    return pos;
}

int snprintf(char* buf, size_t size, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int ret = vsnprintf(buf, size, fmt, args);
    va_end(args);
    return ret;
}