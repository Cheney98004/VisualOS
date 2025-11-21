#ifndef KSTRING_H
#define KSTRING_H

#include <stddef.h>

void *k_memcpy(void *dst, const void *src, size_t n);
void *k_memset(void *dst, int c, size_t n);

int    k_strcmp(const char *a, const char *b);
char  *k_strcpy(char *dst, const char *src);
size_t k_strlen(const char *s);

char  *k_strrchr(const char *s, int c);
char  *k_strtok(char *s, const char *delim);

int    k_snprintf(char *buf, size_t size, const char *fmt, ...);

#endif
