/*
 * Copyright (c) 2014-2018, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <stdio.h>

/*
 * Print a formatted string on the UART.
 *
 * Does the same thing as the standard libc's printf() function but in a MP-safe
 * manner, i.e. it can be called from several CPUs simultaneously without
 * getting interleaved messages.
 */
__attribute__((format(printf, 1, 2)))
void mp_printf(const char *fmt, ...);

/*
 * The log output macros print output to the console. These macros produce
 * compiled log output only if the LOG_LEVEL defined in the makefile (or the
 * make command line) is greater or equal than the level required for that
 * type of log output.
 * The format expected is similar to printf(). For example:
 * INFO("Info %s.\n", "message")    -> INFO: Info message.
 * WARN("Warning %s.\n", "message") -> WARNING: Warning message.
 */
#define LOG_LEVEL_NONE                  0
#define LOG_LEVEL_ERROR                 10
#define LOG_LEVEL_NOTICE                20
#define LOG_LEVEL_WARNING               30
#define LOG_LEVEL_INFO                  40
#define LOG_LEVEL_VERBOSE               50

#if LOG_LEVEL >= LOG_LEVEL_NOTICE
# define NOTICE(...)    mp_printf("NOTICE:  " __VA_ARGS__)
#else
# define NOTICE(...)
#endif

#if LOG_LEVEL >= LOG_LEVEL_ERROR
# define ERROR(...)     mp_printf("ERROR:   " __VA_ARGS__)
#else
# define ERROR(...)
#endif

#if LOG_LEVEL >= LOG_LEVEL_WARNING
# define WARN(...)      mp_printf("WARNING: " __VA_ARGS__)
#else
# define WARN(...)
#endif

#if LOG_LEVEL >= LOG_LEVEL_INFO
# define INFO(...)      mp_printf("INFO:    " __VA_ARGS__)
#else
# define INFO(...)
#endif

#if LOG_LEVEL >= LOG_LEVEL_VERBOSE
# define VERBOSE(...)	mp_printf("VERBOSE: " __VA_ARGS__)
#else
# define VERBOSE(...)
#endif

/*
 * For the moment this Panic function is very basic, Report an error and
 * spin. This can be expanded in the future to provide more information.
 */
#if DEBUG
void __attribute__((__noreturn__)) do_panic(const char *file, int line);
#define panic()	do_panic(__FILE__, __LINE__)

void __attribute__((__noreturn__)) do_bug_unreachable(const char *file, int line);
#define bug_unreachable() do_bug_unreachable(__FILE__, __LINE__)

#else
void __attribute__((__noreturn__)) do_panic(void);
#define panic()	do_panic()

void __attribute__((__noreturn__)) do_bug_unreachable(void);
#define bug_unreachable()	do_bug_unreachable()

#endif

#endif /* __DEBUG_H__ */
