#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void *memcpy(void *restrict dest, const void *restrict src, size_t n);
void *memset(void *s, int c, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);

int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
void strcpy(char *s1, const char *s2);
size_t strlen(const char *str);
int streq(const char *a, const char *b);
void strcat(char *dst, const char *src);
char *strncpy(char *dest, const char *src, size_t n);
int snprintf(char *buf, size_t size, const char *fmt, ...);