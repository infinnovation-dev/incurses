/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Infinnovation Ltd
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/*=======================================================================
 *	Minimal (n)curses for Atto
 *=======================================================================*/
#include "curses.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ESC "\x1b"

/* Assume there's only ever one window, the whole thing */
WINDOW *stdscr = (WINDOW *)1;
WINDOW *curscr = (WINDOW *)1;
attr_t incurses_pairs[COLOR_PAIRS];

/* Private global data */
static struct {
    int started;                        /* incurses running? */
    int echo;                           /* local echo of input */
    int keypad;                         /* recognise e.g ESC[A as keys? */
    attr_t attr;                        /* current colour and attributes */
    uint8_t x, y;                       /* current column end row */
} incur_global = {
    .started = false,
    .echo = true,
    .keypad = false,
    .attr = 0x00,
    .x = 0,
    .y = LINES-1,
};

/* Shortcut to private globals data */
#define G(v) (incur_global.v)

/* Extract colours from attributes */
#define GET_FG(a) ((((a) & INCURSES_FG_MASK) >> INCURSES_FG_SHIFT) - 1)
#define GET_BG(a) ((((a) & INCURSES_BG_MASK) >> INCURSES_BG_SHIFT) - 1)

/* Forward declarations */
static void _move(unsigned y, unsigned x);

#ifdef DEBUG
/*-----------------------------------------------------------------------
 *	Debug
 *-----------------------------------------------------------------------*/
#include <stdarg.h>
static bool dbg_esc = false;
static FILE *dbg = NULL;
static void DBGOPEN(void)
{
    if ((dbg = fopen("incur.log","w")) == NULL) {
        perror("open incur.log");
        exit(2);
    }
}

static void DBG(const char *fmt, ...)
{
    va_list ap;
    if (dbg == NULL) DBGOPEN();
    va_start(ap, fmt);
    vfprintf(dbg, fmt, ap);
    fputc('\n', dbg);
    fflush(dbg);
    va_end(ap);
}

static void
DBGC(uint8_t c)
{
    if (dbg == NULL) DBGOPEN();
    if (dbg_esc) {
        fputc(c, dbg);
        if (('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z')) {
            fputs("\"\n", dbg);
            dbg_esc = false;
        }
    } else if (c == 0x1b) {
        fputs("out \"\\e", dbg);
        dbg_esc = true;
    }
}

#else
/* No debug */
#define DBG(...)
#define DBGC(c)
#endif

#if defined(__MBED__)
/*-----------------------------------------------------------------------
 *      mbed serial driver
 *
 *	Use the 'stdio_uart' global variable by which mbed connects each
 *	target's serial implementation with the C library stdio.
 *	See mbed-os/platform/mbed_retarget.cpp and e.g.
 *	mbed-os/targets/TARGET_NXP/TARGET_LPC176X/serial_api.c
 *	N.B. This is not an advertised part of the API so may break
 *	in a future release of mbed.
 *-----------------------------------------------------------------------*/
#undef ERR                              /* Conflict with MK64F12 DMA register */
#include "serial_api.h"
extern int stdio_uart_inited;
extern serial_t stdio_uart;

static int
_mbedserial_getc(int timeout_ms)
{
    /* FIXME honour timeout */
    return serial_getc(&stdio_uart);
}


static void
_mbedserial_putc(int c)
{
    DBGC(c);
    serial_putc(&stdio_uart, c);
}

static void
_mbedserial_puts(const char *str)
{
    int c;
    while ((c = *str++) != 0) {
        _mbedserial_putc(c);
    }
}

#define DRV_RAW(bf)
#define DRV_ECHO(bf)
#define DRV_GETC _mbedserial_getc
#define DRV_FLUSHIN()
#define DRV_PUTC _mbedserial_putc
#define DRV_PUTS _mbedserial_puts
#define DRV_FLUSH()

#else
/*-----------------------------------------------------------------------
 *	Unix terminal driver
 *-----------------------------------------------------------------------*/
#include <termios.h>

static void
unixterm_raw(bool flag)
{
    struct termios t;
    tcgetattr(1, &t);
    if (flag) {
        t.c_lflag &= ~ (ICANON | ISIG);
    } else {
        t.c_lflag |= (ICANON | ISIG);
    }
    tcsetattr(1, TCSAFLUSH, &t);
}

static void
unixterm_echo(bool flag)
{
    struct termios t;
    tcgetattr(1, &t);
    if (flag) {
        t.c_lflag |= (ECHO);
    } else {
        t.c_lflag &= ~ (ECHO);
    }
    tcsetattr(1, TCSAFLUSH, &t);
}

static int
unixterm_getc(int timeout_ms)
{
    /* FIXME honour timeout */
    return getchar();
}

static void
unixterm_putc(int c)
{
    DBGC(c);
    fputc(c, stdout);
}

static void
unixterm_puts(const char *str)
{
    // DBG("puts \"%s\"", str);
    {const char *s=str; while (*s++) DBGC(s[-1]);}
    fputs(str, stdout);
}

#define DRV_RAW unixterm_raw
#define DRV_ECHO unixterm_echo
#define DRV_GETC unixterm_getc
#define DRV_FLUSHIN()
#define DRV_PUTC unixterm_putc
#define DRV_PUTS unixterm_puts
#define DRV_FLUSH() fflush(stdout)

#endif

/*-----------------------------------------------------------------------
 *	initscr
 *-----------------------------------------------------------------------*/
WINDOW *
initscr(void)
{
    DRV_ECHO(false);
    attrset(A_NORMAL);
    clear();
    move(0,0);
    G(started) = true;
    return stdscr;
}

int
endwin(void)
{
    move(LINES-1, 0);
    attrset(A_NORMAL);
    clrtoeol();
    curs_set(true);
    DRV_PUTS(ESC"[4l");                 /* set replace mode */
    refresh();
    DRV_ECHO(true);
    G(started) = false;
    return OK;
}

int
idlok(WINDOW *win, bool bf)
{
    return OK;
}

int
start_color(void)
{
    return OK;
}

int
init_pair(unsigned pair, uint8_t fg, uint8_t bg)
{
    if (pair == 0 || pair >= COLOR_PAIRS) return INCURSES_ERR;
    if (fg >= 8 || bg >= 8) return INCURSES_ERR;
    DBG("init_pair %u %u %u", pair, fg, bg);
    incurses_pairs[pair] = INCURSES_FG(fg) | INCURSES_BG(bg);
    return OK;
}

/*-----------------------------------------------------------------------
 *	input
 *-----------------------------------------------------------------------*/
int
getch(void)
{
    int ch = DRV_GETC(-1);
    /* FIXME if G(keypad), recognise ESC sequences */
    /* FIXME if G(echo), output character */
    return ch;
}

int
flushinp(void)
{
    DRV_FLUSHIN();
    return OK;
}

int
noraw(void)
{
    DRV_RAW(false);
    return OK;
}

int
raw(void)
{
    DRV_RAW(true);
    return OK;
}

int
noecho(void)
{
    G(echo) = 0;
    return OK;
}

int
keypad(WINDOW *win, bool bf)
{
    G(keypad) = bf;
    return OK;
}

/*-----------------------------------------------------------------------
 *	output
 *-----------------------------------------------------------------------*/
int
curs_set(bool visible)
{
    if (visible) {
        DRV_PUTS(ESC"[?25h");
    } else {
        DRV_PUTS(ESC"[?25l");
    }
    return OK;
}

int
addch(uint8_t ch)
{
    switch (ch) {
    case 0x08:				/* Left unless at first column */
        if (G(x) > 0) {
            DRV_PUTC(ch);
            -- G(x);
        }
        break;
    case 0x09:				/* Move to 8-sized tab stop */
        /* FIXME */
        break;
    case 0x0a:
        clrtoeol();
        DRV_PUTC(ch);
        ++ G(y);
        G(x) = 0;
        break;
    case 0x0d:				/* Move to first column */
        DRV_PUTC(ch);
        G(x) = 0;
        break;
    default:
        if (ch < 0x20 || ch >= 0x7f) {
            /* FIXME */
            DBG("addch %02x", ch);
            DRV_PUTC(ch);
        } else {
            DRV_PUTC(ch);
            ++ G(x);
        }
    }
    return OK;
}

int
addnstr(const char *str, int n)
{
    if (n < 0) n = strlen(str);
    DBG("addstr \"%.*s\"", n, str);
    while (n-- > 0) {
        chtype c = *str++;
        addch(c);
    }
    return OK;
}

int
attr_on(attr_t on, void *opts)
{
    attr_t attr = G(attr);
    attr |= on & INCURSES_ATTR_MASK;
    if (on & INCURSES_FG_MASK) {
        attr = (attr & ~ INCURSES_FG_MASK) | (on & INCURSES_FG_MASK);
    }
    if (on & INCURSES_BG_MASK) {
        attr = (attr & ~ INCURSES_BG_MASK) | (on & INCURSES_BG_MASK);
    }
    return attrset(attr);
}

int
attrset(attr_t attr)
{
    if (attr != G(attr)) {
        DBG("attrset %04x", attr);
        DRV_PUTS(ESC"[0");
        if (attr & INCURSES_FG_MASK) {
            DRV_PUTS(";3");
            DRV_PUTC('0' + GET_FG(attr));
        }
        if (attr & INCURSES_BG_MASK) {
            DRV_PUTS(";4");
            DRV_PUTC('0' + GET_BG(attr));
        }
        if (attr & A_UNDERLINE) {
            DRV_PUTS(";4");
        }
        if (attr & A_REVERSE) {
            DRV_PUTS(";7");
        }
        DRV_PUTC('m');

        G(attr) = attr;
    }
    return OK;
}

int
clear(void)
{
    DRV_PUTS(ESC"[2J");                 /* clear screen */
    return OK;
}

int
clrtoeol(void)
{
    DRV_PUTS(ESC"[K");                  /* clear to end of line */
    return OK;
}

int
move(int y, int x)
{
    if (! (x == G(x) && y == G(y))) {
        DBG("move %d %d", y, x);
        G(x) = x;
        G(y) = y;
        _move(y, x);
    }
    return OK;
}

int
refresh(void)
{
    DRV_FLUSH();
    return OK;
}

char *
unctrl(chtype c)
{
    static char repr[4+1];
    if (c >= 0x20 && c < 0x7f) {
        repr[0] = c;
        repr[1] = 0;
    } else if (c < 0x80) {
        repr[0] = '^';
        repr[1] = c ^ 0x40;
        repr[2] = 0;
    } else if (c >= 0xa0 && c < 0xff) {
        repr[0] = 'M';
        repr[1] = '-';
        repr[2] = c ^ 0x80;
        repr[3] = 0;
    } else {
        repr[0] = '~';
        repr[1] = c ^ 0xc0;
        repr[2] = 0;
    }
    return repr;
}

/*-----------------------------------------------------------------------
 *	internals
 *-----------------------------------------------------------------------*/
static void
_move(unsigned y, unsigned x)
{
    char cmd[10];
    sprintf(cmd, ESC"[%u;%uH", y+1, x+1);
    DRV_PUTS(cmd);
}

/* end */
