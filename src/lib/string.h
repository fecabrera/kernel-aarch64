#ifndef INCLUDE_H
#define INCLUDE_H

#include <types.h>

typedef uint64_t size_t;

void *memcpy(void *destination, const void *source, size_t num);
void *memmove(void *destination, const void *source, size_t num);
char *strcpy(char *destination, const char *source);
char *strncpy(char *destination, const char *source, size_t num);

int memcmp(const void *ptr1, const void *ptr2, size_t num);
int strcmp(const char *str1, const char *str2);
int strncmp(const char *str1, const char *str2, size_t num);

size_t strlen(const char *str);

#endif // INCLUDE_H