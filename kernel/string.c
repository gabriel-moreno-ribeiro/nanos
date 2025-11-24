#include "kernel.h"

void *memset(void *dst, int value, size_t n) {
    uint8_t *d = dst;
    while (n--) *d++ = (uint8_t)value;
    return dst;
}

void *memcpy(void *dst, const void *src, size_t n) {
    uint8_t *d = dst;
    const uint8_t *s = src;
    while (n--) *d++ = *s++;
    return dst;
}

size_t strlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n) {
    while (n && *a && *a == *b) { a++; b++; n--; }
    return n ? (unsigned char)*a - (unsigned char)*b : 0;
}

/* Writes value in the given base into buf (at least 33 bytes) and returns buf. */
char *utoa(uint32_t value, char *buf, unsigned base) {
    static const char digits[] = "0123456789abcdef";
    char tmp[33];
    int i = 0;
    do {
        tmp[i++] = digits[value % base];
        value /= base;
    } while (value);
    int j = 0;
    while (i) buf[j++] = tmp[--i];
    buf[j] = 0;
    return buf;
}

int atoi(const char *s) {
    int sign = 1, v = 0;
    while (*s == ' ') s++;
    if (*s == '-') { sign = -1; s++; }
    while (*s >= '0' && *s <= '9') v = v * 10 + (*s++ - '0');
    return sign * v;
}
