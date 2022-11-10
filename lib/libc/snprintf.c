/*
 * Copyright (c) 2017-2022, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>

#include <common/debug.h>

#define get_num_va_args(_args, _lcount)				\
	(((_lcount) > 1)  ? va_arg(_args, long long int) :	\
	(((_lcount) == 1) ? va_arg(_args, long int) :		\
			    va_arg(_args, int)))

#define get_unum_va_args(_args, _lcount)				\
	(((_lcount) > 1)  ? va_arg(_args, unsigned long long int) :	\
	(((_lcount) == 1) ? va_arg(_args, unsigned long int) :		\
			    va_arg(_args, unsigned int)))

static void string_print(char **s, size_t n, size_t *chars_printed,
			 const char *str)
{
	while (*str != '\0') {
		if (*chars_printed < n) {
			*(*s) = *str;
			(*s)++;
		}

		(*chars_printed)++;
		str++;
	}
}

static void unsigned_num_print(char **s, size_t n, size_t *count,
			      unsigned long long int unum, unsigned int radix,
			      char padc, int padn)
{
	/* Just need enough space to store 64 bit decimal integer */
	char num_buf[20];
	int i = 0;
	int width;
	unsigned int rem;

	do {
		rem = unum % radix;
		if (rem < 0xa)
			num_buf[i] = '0' + rem;
		else
			num_buf[i] = 'a' + (rem - 0xa);
		i++;
		unum /= radix;
	} while (unum > 0U);

	width = i;

	if (padn > 0) {
		while (width < padn) {
			if (*count < n) {
				*(*s) = padc;
				(*s)++;
			}
			(*count)++;
			padn--;
		}
	}

	while (--i >= 0) {
		if (*count < n) {
			*(*s) = num_buf[i];
			(*s)++;
		}
		(*count)++;
	}

	if (padn < 0) {
		while (width < -padn) {
			if (*count < n) {
				*(*s) = padc;
				(*s)++;
			}
			(*count)++;
			padn++;
		}
	}
}

/*
 * Scaled down version of vsnprintf(3).
 */
int vsnprintf(char *s, size_t n, const char *fmt, va_list args)
{
	int l_count;
	int left;
	char *str;
	int num;
	unsigned long long int unum;
	char padc; /* Padding character */
	int padn; /* Number of characters to pad */
	size_t count = 0U;

	if (n == 0U) {
		/* There isn't space for anything. */
	} else if (n == 1U) {
		/* Buffer is too small to actually write anything else. */
		*s = '\0';
		n = 0U;
	} else {
		/* Reserve space for the terminator character. */
		n--;
	}

	while (*fmt != '\0') {
		l_count = 0;
		left = 0;
		padc = '\0';
		padn = 0;

		if (*fmt == '%') {
			fmt++;
			/* Check the format specifier. */
loop:
			switch (*fmt) {
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				padc = ' ';
				for (padn = 0; *fmt >= '0' && *fmt <= '9'; fmt++)
					padn = (padn * 10) + (*fmt - '0');
				if (left)
					padn = -padn;
				goto loop;
			case '-':
				left = 1;
				fmt++;
				goto loop;
			case 'i':
			case 'd':
				num = get_num_va_args(args, l_count);

				if (num < 0) {
					if (count < n) {
						*s = '-';
						s++;
					}
					count++;

					unum = (unsigned int)-num;
				} else {
					unum = (unsigned int)num;
				}

				unsigned_num_print(&s, n, &count, unum, 10,
						   padc, padn);
				break;
			case 'l':
				l_count++;
				fmt++;
				goto loop;
			case 's':
				str = va_arg(args, char *);
				string_print(&s, n, &count, str);
				break;
			case 'u':
				unum = get_unum_va_args(args, l_count);
				unsigned_num_print(&s, n, &count, unum, 10,
						   padc, padn);
				break;
			case 'x':
				unum = get_unum_va_args(args, l_count);
				unsigned_num_print(&s, n, &count, unum, 16,
						   padc, padn);
				break;
			case '0':
				padc = '0';
				padn = 0;
				fmt++;

				for (;;) {
					char ch = *fmt;
					if ((ch < '0') || (ch > '9')) {
						goto loop;
					}
					padn = (padn * 10) + (ch - '0');
					fmt++;
				}
				assert(0); /* Unreachable */
			default:
				/*
				 * Exit on any other format specifier and abort
				 * when in debug mode.
				 */
				WARN("snprintf: specifier with ASCII code '%d' not supported.\n",
				     *fmt);
				assert(0);
				return -1;
			}
			fmt++;
			continue;
		}

		if (count < n) {
			*s = *fmt;
			s++;
		}

		fmt++;
		count++;
	}

	if (n > 0U)
		*s = '\0';

	return (int)count;
}

/*******************************************************************
 * Reduced snprintf to be used for Trusted firmware.
 * The following type specifiers are supported:
 *
 * %d or %i - signed decimal format
 * %s - string format
 * %u - unsigned decimal format
 *
 * The function panics on all other formats specifiers.
 *
 * It returns the number of characters that would be written if the
 * buffer was big enough. If it returns a value lower than n, the
 * whole string has been written.
 *******************************************************************/
int snprintf(char *s, size_t n, const char *fmt, ...)
{
	va_list args;
	int chars_printed;

	va_start(args, fmt);
	chars_printed = vsnprintf(s, n, fmt, args);
	va_end(args);

	return chars_printed;
}
