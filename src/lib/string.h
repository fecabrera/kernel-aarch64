#ifndef INCLUDE_H
#define INCLUDE_H

#include <types.h>

typedef uint64_t size_t;

/***************************************
 * String manipulation
 ***************************************/

/**
 * Copies the null-terminated string pointed to by src into dest, including the null terminator.
 * The objects must not overlap.
 *
 * @param dest: pointer to the character array to copy to
 * @param src: pointer to the null-terminated string to copy from
 *
 * @return dest
 */
char *strcpy(char *dest, const char *src);

/**
 * Copies at most count characters from src into dest. If src is shorter than count,
 * the remaining characters in dest are padded with null bytes.
 *
 * @param dest: pointer to the character array to copy to
 * @param src: pointer to the null-terminated string to copy from
 * @param count: maximum number of characters to copy
 *
 * @return dest
 */
char *strncpy(char *dest, const char *src, size_t count);

/***************************************
 * String examination
 ***************************************/

/**
 * Returns the length of the null-terminated string str, not including the null terminator.
 *
 * @param str: pointer to the null-terminated string to measure
 *
 * @return Number of characters in str.
 */
size_t strlen(const char *str);

/**
 * Compares two null-terminated strings lexicographically.
 *
 * @param lhs: pointer to the first null-terminated string
 * @param rhs: pointer to the second null-terminated string
 *
 * @return Negative if lhs < rhs, zero if equal, positive if lhs > rhs.
 */
int strcmp(const char *lhs, const char *rhs);

/**
 * Compares at most count characters of two null-terminated strings lexicographically.
 *
 * @param lhs: pointer to the first null-terminated string
 * @param rhs: pointer to the second null-terminated string
 * @param count: maximum number of characters to compare
 *
 * @return Negative if lhs < rhs, zero if equal or count is zero, positive if lhs > rhs.
 */
int strncmp(const char *lhs, const char *rhs, size_t count);

/***************************************
 * Character array manipulation
 ***************************************/

/**
 * Compares the first count bytes of the objects pointed to by lhs and rhs lexicographically.
 * The sign of the result is the sign of the difference between the first pair of differing bytes.
 * Behavior is undefined if either pointer is null or access goes beyond the end of either object.
 *
 * @param lhs: pointer to the first object
 * @param rhs: pointer to the second object
 * @param count: number of bytes to compare
 *
 * @return Negative if lhs < rhs, zero if equal or count is zero, positive if lhs > rhs.
 */
int memcmp(const void *lhs, const void *rhs, size_t count);

/**
 * Fills the first count bytes of the object pointed to by dest with the value ch.
 *
 * @param dest: pointer to the object to fill
 * @param ch: fill byte value (converted to unsigned char)
 * @param count: number of bytes to fill
 *
 * @return dest
 */
void *memset(void *dest, int ch, size_t count);

/**
 * Copies count bytes from src to dest. The objects must not overlap.
 *
 * @param dest: pointer to the object to copy to
 * @param src: pointer to the object to copy from
 * @param count: number of bytes to copy
 *
 * @return dest
 */
void *memcpy(void *dest, const void *src, size_t count);

/**
 * Copies count bytes from src to dest. The objects may overlap.
 *
 * @param dest: pointer to the object to copy to
 * @param src: pointer to the object to copy from
 * @param count: number of bytes to copy
 *
 * @return dest
 */
void *memmove(void *dest, const void *src, size_t count);

#endif // INCLUDE_H