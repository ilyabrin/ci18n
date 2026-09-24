/*
 * stdout for the AVR tests: USART0, which QEMU writes to a file. On any
 * other target these do nothing, so a test can run on the host as well.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef CI18N_TEST_CONSOLE_H
#define CI18N_TEST_CONSOLE_H

#include <stdio.h>

#ifdef __AVR__
#include <avr/io.h>

static int console_put(char c, FILE *stream)
{
    (void)stream;
    while (!(UCSR0A & (1 << UDRE0)))
    {
    }
    UDR0 = (uint8_t)c;
    return 0;
}

static FILE console = FDEV_SETUP_STREAM(console_put, NULL, _FDEV_SETUP_WRITE);

static void console_open(void)
{
    UCSR0B = 1 << TXEN0;
    stdout = &console;
}

/* There is nothing to return to. The runner stops QEMU once it has read
 * the summary. */
#define console_exit(status) \
    do                       \
    {                        \
        (void)(status);      \
        for (;;)             \
        {                    \
        }                    \
    } while (0)
#else
#define console_open() ((void)0)
#define console_exit(status) return (status)
#endif

#endif
