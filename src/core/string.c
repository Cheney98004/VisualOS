#include "string.h"
#include <stdarg.h>
#include <stdint.h>

void *k_memcpy(void *dst, const void *src, size_t n) {
    unsigned char *d = dst;
    const unsigned char *s = src;
    while (n--) *d++ = *s++;
    return dst;
}

void *k_memset(void *dst, int c, size_t n) {
    unsigned char *d = dst;
    while (n--) *d++ = (unsigned char)c;
    return dst;
}

int k_strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) a++, b++;
    return (unsigned char)*a - (unsigned char)*b;
}

char *k_strcpy(char *dst, const char *src) {
    char *r = dst;
    while ((*dst++ = *src++));
    return r;
}

size_t k_strlen(const char *s) {
    size_t n = 0;
    while (s && *s++) n++;
    return n;
}

char *k_strrchr(const char *s, int c)
{
    const char *last = NULL;
    while (*s) {
        if (*s == c) last = s;
        s++;
    }
    return (char*)last;
}

static int is_delim(char c, const char *d)
{
    while (*d) {
        if (c == *d) return 1;
        d++;
    }
    return 0;
}

char *k_strtok(char *s, const char *delim)
{
    static char *p;

    if (s)
        p = s;

    if (!p)
        return NULL;

    while (*p && is_delim(*p, delim))
        p++;

    if (!*p)
        return NULL;

    char *start = p;

    while (*p && !is_delim(*p, delim))
        p++;

    if (*p)
        *p++ = 0;

    return start;
}

int k_snprintf(char *buf, size_t size, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    size_t pos = 0;

    while (*fmt && pos < size - 1) {

        if (*fmt != '%') {
            buf[pos++] = *fmt++;
            continue;
        }

        fmt++;

        if (*fmt == 's') {
            const char *str = va_arg(ap, const char *);
            while (*str && pos < size - 1)
                buf[pos++] = *str++;
        }
        else if (*fmt == 'd') {
            int v = va_arg(ap, int);
            char tmp[32];
            int i = 0;
            int neg = (v < 0);
            if (neg) v = -v;

            do {
                tmp[i++] = '0' + (v % 10);
                v /= 10;
            } while (v && i < 31);

            if (neg) tmp[i++] = '-';

            while (i-- && pos < size - 1)
                buf[pos++] = tmp[i];
        }
        else if (*fmt == 'u') {
            unsigned v = va_arg(ap, unsigned);
            char tmp[32];
            int i = 0;

            do {
                tmp[i++] = '0' + (v % 10);
                v /= 10;
            } while (v && i < 31);

            while (i-- && pos < size - 1)
                buf[pos++] = tmp[i];
        }
        else if (*fmt == 'c') {
            buf[pos++] = (char)va_arg(ap, int);
        }

        fmt++;
    }

    buf[pos] = 0;
    va_end(ap);
    return pos;
}

int k_memcmp(const void *a, const void *b, uint32_t n) {
    const uint8_t *p1 = (const uint8_t*)a;
    const uint8_t *p2 = (const uint8_t*)b;

    for (uint32_t i = 0; i < n; i++) {
        if (p1[i] != p2[i])
            return (int)p1[i] - (int)p2[i];
    }
    return 0;
}

char *k_strchr(const char *s, int c) {
    while (*s) {
        if (*s == c) return (char*)s;
        s++;
    }
    return 0;
}