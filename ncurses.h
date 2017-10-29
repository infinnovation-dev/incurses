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
 *	Minimal (n)curses implementation.
 *	Only enough to be able to run Atto, a small Emacs-like editor.
 *	Don't expect miracles.
 *=======================================================================*/
#ifndef INC_ncurses_h
#define INC_ncurses_h

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Types */
typedef struct _WINDOW WINDOW;
typedef uint8_t chtype;
typedef uint16_t attr_t;

/* Constants */
#ifndef OK
#define OK 0
#endif

#ifndef ERR
#define ERR 1
#endif

/* Atto expects these */
#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

/* Assume fixed screen size */
#define LINES 24
#define COLS 80

enum {
  A_NORMAL    = ((attr_t)0x0000),
  A_UNDERLINE = ((attr_t)0x0001),
  A_REVERSE   = ((attr_t)0x0002),
  A_STANDOUT  = A_REVERSE
};

#define INCURSES_ATTR_MASK 0x00ff
#define INCURSES_FG_SHIFT 8
#define INCURSES_FG_MASK  0x0f00
#define INCURSES_BG_SHIFT 12
#define INCURSES_BG_MASK  0xf000

#define INCURSES_FG(c) (((c)+1) << INCURSES_FG_SHIFT)
#define INCURSES_BG(c) (((c)+1) << INCURSES_BG_SHIFT)

enum {
    COLOR_BLACK   = 0,
    COLOR_RED     = 1,
    COLOR_GREEN   = 2,
    COLOR_YELLOW  = 3,
    COLOR_BLUE    = 4,
    COLOR_MAGENTA = 5,
    COLOR_CYAN    = 6,
    COLOR_WHITE   = 7
};

#define COLOR_PAIRS 16

/* Visible data */
extern WINDOW *stdscr;
extern WINDOW *curscr;
extern attr_t incurses_pairs[COLOR_PAIRS];

/* Macros */
#define COLOR_PAIR(p) ((unsigned)(p) >= COLOR_PAIRS ? (attr_t)0 : incurses_pairs[p])
#define addstr(s) addnstr(s, -1)
#define attron(a) attr_on(a, 0)
#define mvaddstr(y,x,s) (move(y,x), addstr(s))
#define standout() attron(A_STANDOUT)
#define standend() attrset(A_NORMAL)

/* API functions */
extern WINDOW *initscr(void);
extern int endwin(void);
extern int idlok(WINDOW *, bool);
extern int start_color(void);
extern int init_pair(unsigned, uint8_t, uint8_t);
extern int getch(void);
extern int flushinp(void);
extern int noraw(void);
extern int raw(void);
extern int noecho(void);
extern int keypad(WINDOW *, bool);
extern int curs_set(bool);
extern int addch(chtype);
extern int addnstr(const char *, int n);
extern int attr_on(attr_t, void *);
extern int attrset(attr_t);
extern int clear(void);
extern int clrtoeol(void);
extern int move(int, int);
extern int refresh(void);
extern char *unctrl(chtype);

#ifdef __cplusplus
}
#endif
#endif /* INC_ncurses_h */
