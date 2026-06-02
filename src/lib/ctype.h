#pragma once

/**
 * Tests whether c is an alphanumeric character (A–Z, a–z, or 0–9).
 *
 * @param c: character to test (as unsigned char value or EOF)
 *
 * @return non-zero if c is alphanumeric, zero otherwise
 */
int isalnum(int c);

/**
 * Tests whether c is an alphabetic character (A–Z or a–z).
 *
 * @param c: character to test (as unsigned char value or EOF)
 *
 * @return non-zero if c is alphabetic, zero otherwise
 */
int isalpha(int c);

/**
 * Tests whether c is a blank character (space or horizontal tab).
 *
 * @param c: character to test (as unsigned char value or EOF)
 *
 * @return non-zero if c is blank, zero otherwise
 */
int isblank(int c);

/**
 * Tests whether c is a control character (0x00–0x1F or 0x7F).
 *
 * @param c: character to test (as unsigned char value or EOF)
 *
 * @return non-zero if c is a control character, zero otherwise
 */
int iscntrl(int c);

/**
 * Tests whether c is a decimal digit (0–9).
 *
 * @param c: character to test (as unsigned char value or EOF)
 *
 * @return non-zero if c is a digit, zero otherwise
 */
int isdigit(int c);

/**
 * Tests whether c is a printable non-space character (0x21–0x7E).
 *
 * @param c: character to test (as unsigned char value or EOF)
 *
 * @return non-zero if c has a visible glyph, zero otherwise
 */
int isgraph(int c);

/**
 * Tests whether c is a lowercase letter (a–z).
 *
 * @param c: character to test (as unsigned char value or EOF)
 *
 * @return non-zero if c is lowercase, zero otherwise
 */
int islower(int c);

/**
 * Tests whether c is a printable character (0x20–0x7E, including space).
 *
 * @param c: character to test (as unsigned char value or EOF)
 *
 * @return non-zero if c is printable, zero otherwise
 */
int isprint(int c);

/**
 * Tests whether c is a punctuation character (printable, non-alphanumeric, non-space).
 *
 * @param c: character to test (as unsigned char value or EOF)
 *
 * @return non-zero if c is punctuation, zero otherwise
 */
int ispunct(int c);

/**
 * Tests whether c is a whitespace character (space, tab, newline, carriage
 * return, form feed, or vertical tab).
 *
 * @param c: character to test (as unsigned char value or EOF)
 *
 * @return non-zero if c is whitespace, zero otherwise
 */
int isspace(int c);

/**
 * Tests whether c is an uppercase letter (A–Z).
 *
 * @param c: character to test (as unsigned char value or EOF)
 *
 * @return non-zero if c is uppercase, zero otherwise
 */
int isupper(int c);

/**
 * Tests whether c is a hexadecimal digit (0–9, a–f, or A–F).
 *
 * @param c: character to test (as unsigned char value or EOF)
 *
 * @return non-zero if c is a hex digit, zero otherwise
 */
int isxdigit(int c);

/**
 * Converts an uppercase letter to its lowercase equivalent.
 *
 * @param c: character to convert (as unsigned char value or EOF)
 *
 * @return lowercase equivalent of c, or c unchanged if not uppercase
 */
int tolower(int c);

/**
 * Converts a lowercase letter to its uppercase equivalent.
 *
 * @param c: character to convert (as unsigned char value or EOF)
 *
 * @return uppercase equivalent of c, or c unchanged if not lowercase
 */
int toupper(int c);
