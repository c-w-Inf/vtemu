#include "mvterm.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <vterm.h>

#include "vterm_keycodes.h"

#define MVTERM_PRINT_COLOR 1
#define MVTERM_PRINT_WIDTH 2
#define MVTERM_PRINT_VISUAL 4
#define MVTERM_PRINT_PRETTY 8
#define MVTERM_PRINT_VISUALM 7
#define MVTERM_PRINT_PRETTYM 15

static void pututf8 (uint32_t cp) {
    if (cp < 0x80) {
        putchar ((int)cp);
    } else if (cp < 0x800) {
        putchar (0xC0 | (int)(cp >> 6));
        putchar (0x80 | (int)(cp & 0x3F));
    } else if (cp < 0x10000) {
        putchar (0xE0 | (int)(cp >> 12));
        putchar (0x80 | (int)((cp >> 6) & 0x3F));
        putchar (0x80 | (int)(cp & 0x3F));
    } else {
        putchar (0xF0 | (int)(cp >> 18));
        putchar (0x80 | (int)((cp >> 12) & 0x3F));
        putchar (0x80 | (int)((cp >> 6) & 0x3F));
        putchar (0x80 | (int)(cp & 0x3F));
    }
}
static void puthexdig (int hex) {
    if (hex >= 10)
        putchar (hex - 10 + 'a');
    else
        putchar (hex + '0');
}
static void putcsi (int id, char type, int args) {
    if (args & MVTERM_PRINT_VISUAL) {
        putchar ('\x1b'), putchar ('[');
        if (id >= 0) {
            if (id >= 10) putchar (id / 10 + '0');
            putchar (id % 10 + '0');
        }
        putchar (type);
    } else {
        putchar ('%');
        putchar ('c');
        if (id >= 0) {
            putchar (id / 10 + '0');
            putchar (id % 10 + '0');
        } else {
            putchar ('x');
            putchar ('x');
        }
        putchar (type);
    }
}
static void print_attr (VTermScreenCellAttrs* attr, VTermScreenCellAttrs* nattr, int args) {
    if (!(args & MVTERM_PRINT_COLOR)) return;
    if (!attr->bold && nattr->bold) {
        putcsi (1, 'm', args);
    } else if (attr->bold && !nattr->bold) {
        putcsi (22, 'm', args);
    }
    if (!attr->underline && nattr->underline) {
        putcsi (4, 'm', args);
    } else if (attr->underline && !nattr->underline) {
        putcsi (24, 'm', args);
    }
    if (!attr->italic && nattr->italic) {
        putcsi (3, 'm', args);
    } else if (attr->italic && !nattr->italic) {
        putcsi (23, 'm', args);
    }
    if (!attr->blink && nattr->blink) {
        putcsi (5, 'm', args);
    } else if (attr->blink && !nattr->blink) {
        putcsi (25, 'm', args);
    }
    if (!attr->reverse && nattr->reverse) {
        putcsi (7, 'm', args);
    } else if (attr->reverse && !nattr->reverse) {
        putcsi (27, 'm', args);
    }
    if (!attr->conceal && nattr->conceal) {
        putcsi (8, 'm', args);
    } else if (attr->conceal && !nattr->conceal) {
        putcsi (28, 'm', args);
    }
    if (!attr->strike && nattr->strike) {
        putcsi (9, 'm', args);
    } else if (attr->strike && !nattr->strike) {
        putcsi (29, 'm', args);
    }
    if (attr->font != nattr->font) {
        putcsi (10 + nattr->font, 'm', args);
    }
}
static void print_color (VTermColor* clr, int isfg, int args) {
    if (!(args & MVTERM_PRINT_COLOR)) return;
    if (VTERM_COLOR_IS_RGB (clr)) {
        if (args & MVTERM_PRINT_VISUAL) {
            putchar ('\x1b'), putchar ('[');
            putchar (isfg ? '3' : '4');
            putchar ('8'), putchar (';'), putchar ('2'), putchar (';');
            if (clr->rgb.red >= 100) putchar (clr->rgb.red / 100 + '0');
            if (clr->rgb.red >= 10) putchar (clr->rgb.red / 10 % 10 + '0');
            putchar (clr->rgb.red % 10 + '0');
            putchar (';');
            if (clr->rgb.green >= 100) putchar (clr->rgb.green / 100 + '0');
            if (clr->rgb.green >= 10) putchar (clr->rgb.green / 10 % 10 + '0');
            putchar (clr->rgb.green % 10 + '0');
            putchar (';');
            if (clr->rgb.blue >= 100) putchar (clr->rgb.blue / 100 + '0');
            if (clr->rgb.blue >= 10) putchar (clr->rgb.blue / 10 % 10 + '0');
            putchar (clr->rgb.blue % 10 + '0');
            putchar ('m');
        } else {
            putchar ('%');
            putchar (isfg ? 'f' : 'b');
            puthexdig (clr->rgb.red >> 4);
            puthexdig (clr->rgb.red & 0xf);
            puthexdig (clr->rgb.green >> 4);
            puthexdig (clr->rgb.green & 0xf);
            puthexdig (clr->rgb.blue >> 4);
            puthexdig (clr->rgb.blue & 0xf);
        }
    } else {
        if (args & MVTERM_PRINT_VISUAL) {
            putchar ('\x1b'), putchar ('[');
            putchar (isfg ? '3' : '4');
            putchar ('8'), putchar (';'), putchar ('5'), putchar (';');
            if (clr->indexed.idx >= 100) putchar (clr->indexed.idx / 100 + '0');
            if (clr->indexed.idx >= 10) putchar (clr->indexed.idx / 10 % 10 + '0');
            putchar (clr->indexed.idx % 10 + '0');
            putchar ('m');
        } else {
            putchar ('%');
            putchar (isfg ? 'F' : 'B');
            puthexdig (clr->indexed.idx >> 4);
            puthexdig (clr->indexed.idx & 0xf);
        }
    }
}
static void print_vterm (VTerm* vt, int top, int bottom, int left, int right, int args) {
    VTermState* vtst = vterm_obtain_state (vt);

    VTermColor fg, bg;
    vterm_state_get_default_colors (vtst, &fg, &bg);

    VTermMode mode;
    vterm_state_get_mode (vtst, &mode);

    VTermPos curp;
    vterm_state_get_cursorpos (vtst, &curp);

    print_color (&fg, 1, args);
    print_color (&bg, 0, args);
    if (args & MVTERM_PRINT_PRETTY) putchar ('\x1b'), putchar ('['), putchar ('?'), putchar ('7'), putchar ('l');

    VTermScreenCellAttrs attr = {};
    VTermColor cfg = fg, cbg = bg;

    for (int r = top; r < bottom; r++) {
        for (int c = left; c < right;) {
            VTermScreenCell cell;
            vterm_screen_get_cell (vterm_obtain_screen (vt), (VTermPos){r, c}, &cell);

            print_attr (&attr, &cell.attrs, args);
            attr = cell.attrs;

            if (!vterm_color_is_equal (&cfg, &cell.fg)) {
                cfg = cell.fg;
                print_color (&cfg, 1, args);
            }
            if (!vterm_color_is_equal (&cbg, &cell.bg)) {
                cbg = cell.bg;
                print_color (&cbg, 0, args);
            }

            int on_cursor = r == curp.row && c == curp.col && mode.cursor_visible;

            if (args & MVTERM_PRINT_PRETTY) {
                if (on_cursor) putcsi (7, 'm', args);
                for (int i = 0; i < cell.width; ++i) putchar (' ');

                putcsi (-1, 's', args);
                for (int i = 0; i < cell.width; ++i) putcsi (-1, 'D', args);

            } else if (args & MVTERM_PRINT_VISUAL) {
                if (on_cursor) putcsi (7, 'm', args);

                if (!cell.chars[0]) putchar (' ');

            } else if ((args & MVTERM_PRINT_WIDTH) && (cell.width > 1 || !cell.chars[0] || cell.chars[1])) {
                putchar ('%');
                putchar (cell.width + '0');
                int cpw = 0;
                for (; cpw < VTERM_MAX_CHARS_PER_CELL && cell.chars[cpw]; cpw++) continue;
                putchar (cpw + '0');
            }

            for (int i = 0; i < VTERM_MAX_CHARS_PER_CELL && cell.chars[i]; ++i) {
                uint32_t cp = cell.chars[i];
                if (!(args & MVTERM_PRINT_VISUAL)) {
                    if (cp == '%') putchar ('%');
                }
                pututf8 (cp);
            }
            if (!(args & MVTERM_PRINT_WIDTH) && !cell.chars[0]) putchar (' ');

            if (args & MVTERM_PRINT_VISUAL) {
                if (args & MVTERM_PRINT_PRETTY) putcsi (-1, 'u', args);
                if (on_cursor) putcsi (27, 'm', args);
            }

            c += cell.width;
        }

        putchar ('\n');
    }

    if (args & MVTERM_PRINT_VISUAL) {
        putcsi (0, 'm', args);
        if (args & MVTERM_PRINT_PRETTY) {
            putchar ('\x1b'), putchar ('['), putchar ('!'), putchar ('p');
        }
        fflush (stdout);
    }
}

static int vterm_escape (RINGBUF dest, const char* escape, VTerm* vt) {
    int n;
    char c;
    int x, y, a, b;
    char buf[MVTERM_ESCAPE_MAXLEN];

    VTermKey key = VTERM_KEY_NONE;
    VTermModifier mod = VTERM_MOD_NONE;

#define match(...) (n = -1, sscanf (escape, __VA_ARGS__, &n), (n >= 0 && !escape[n]))
#define headmatch(...) (n = -1, sscanf (escape, __VA_ARGS__, &n), n >= 0)
#define putchars(...)                              \
    do {                                           \
        snprintf (buf, sizeof (buf), __VA_ARGS__); \
        for (char* c = buf; *c; ++c) putchar (*c); \
    } while (0)

    if (headmatch ("P-%n")) {
        escape += 2;
        int args = 0;
        if (headmatch ("C%n")) args |= MVTERM_PRINT_COLOR, ++escape;
        if (headmatch ("W%n")) args |= MVTERM_PRINT_WIDTH, ++escape;

        x = y = 0;
        vterm_get_size (vt, &a, &b);
        if (!match ("%d;%d;%d;%d%n", &x, &y, &a, &b) && !match ("%n")) return -1;

        print_vterm (vt, x, a, y, b, args);
    } else if (match ("L%n")) {
        ringbuf_writed (dest, "<", 1);
    } else if (match ("P%n")) {
        int rows, cols;
        vterm_get_size (vt, &rows, &cols);
        print_vterm (vt, 0, rows, 0, cols, MVTERM_PRINT_PRETTYM);
    } else if (match ("p%n")) {
        int rows, cols;
        vterm_get_size (vt, &rows, &cols);
        print_vterm (vt, 0, rows, 0, cols, MVTERM_PRINT_VISUALM);
    } else if (match ("X%n")) {
        return MVTERM_COMM_PAUSE;
    } else if (match ("E%n")) {
        return MVTERM_COMM_END;
    } else if (match ("TIME%n")) {
        struct timespec now;
        clock_gettime (CLOCK_REALTIME, &now);
        putchars ("%ld.%09ld\n", now.tv_sec, now.tv_nsec);
    } else if (match ("CURM%n")) {
        VTermMode mode;
        vterm_state_get_mode (vterm_obtain_state (vt), &mode);
        putchars (
            "%c\n", (mode.cursor_shape == VTERM_PROP_CURSORSHAPE_BLOCK
                         ? 'O'
                         : (mode.cursor_shape == VTERM_PROP_CURSORSHAPE_BAR_LEFT ? '[' : '_')));
    } else if (match ("CURV%n")) {
        VTermMode mode;
        vterm_state_get_mode (vterm_obtain_state (vt), &mode);
        putchars ("%c\n", (mode.cursor_visible ? 'v' : 'i'));
    } else if (match ("CURB%n")) {
        VTermMode mode;
        vterm_state_get_mode (vterm_obtain_state (vt), &mode);
        putchars ("%c\n", (mode.cursor_blink ? 'b' : 's'));
    } else if (match ("CURP%n")) {
        VTermPos curp;
        vterm_state_get_cursorpos (vterm_obtain_state (vt), &curp);
        putchars ("%d;%d\n", curp.row, curp.col);
    } else if (match ("RSZ%d;%d%n", &x, &y)) {
        vterm_set_size (vt, x, y);
        return MVTERM_COMM_RESIZE;
    } else if (match ("%*c%n")) {
        return -1;
    } else {
        goto key;
    }
    return 0;

key:
    if (headmatch ("M-%n")) {
        escape += 2;
        mod |= VTERM_MOD_ALT;
    }
    if (headmatch ("C-%n")) {
        escape += 2;
        mod |= VTERM_MOD_CTRL;
    }
    if (headmatch ("S-%n")) {
        escape += 2;
        mod |= VTERM_MOD_SHIFT;
    }

    if (match ("%n")) {
        return 0;
    } else if (match ("n%d%n", &x)) {
        c = x;
    } else if (match ("%c%n", &c)) {
    } else if (match ("SP%n")) {
        c = ' ';
    } else if (match ("TAB%n")) {
        key = VTERM_KEY_TAB;
    } else if (match ("CR%n")) {
        key = VTERM_KEY_ENTER;
    } else if (match ("KPCR%n")) {
        key = VTERM_KEY_KP_ENTER;
    } else if (match ("ESC%n")) {
        key = VTERM_KEY_ESCAPE;
    } else if (match ("BS%n")) {
        key = VTERM_KEY_BACKSPACE;
    } else if (match ("UP%n")) {
        key = VTERM_KEY_UP;
    } else if (match ("DOWN%n")) {
        key = VTERM_KEY_DOWN;
    } else if (match ("RIGHT%n")) {
        key = VTERM_KEY_RIGHT;
    } else if (match ("LEFT%n")) {
        key = VTERM_KEY_LEFT;
    } else if (match ("HOME%n")) {
        key = VTERM_KEY_HOME;
    } else if (match ("INS%n")) {
        key = VTERM_KEY_INS;
    } else if (match ("DEL%n")) {
        key = VTERM_KEY_DEL;
    } else if (match ("END%n")) {
        key = VTERM_KEY_END;
    } else if (match ("PGUP%n")) {
        key = VTERM_KEY_PAGEUP;
    } else if (match ("PGDOWN%n")) {
        key = VTERM_KEY_PAGEDOWN;
    } else if (match ("F%d%n", &x)) {
        if (x <= 0 || x > VTERM_KEY_FUNCTION_MAX - VTERM_KEY_FUNCTION_0) return -1;
        key = VTERM_KEY_FUNCTION (x);
    } else {
        return -1;
    }

    if (key != VTERM_KEY_NONE)
        vterm_keyboard_key (vt, key, mod);
    else
        vterm_keyboard_unichar (vt, c, mod);

    return 0;
}

int mvterm_escape_translate (RINGBUF dest, VTERM_STATE* state, char c, VTerm* vt) {
    if (state->buflen == 0) {
        if (c == '<')
            state->buflen = 1;
        else if (c != '\0' && c != '\n' && c != '\r' && c != '\t' && c != ' ')
            ringbuf_writed (dest, &c, 1);

    } else {
        if (c == '>') {
            state->buf[state->buflen - 1] = '\0';
            int ret = vterm_escape (dest, state->buf, vt);
            state->buflen = 0;
            return ret;
        } else if (c == '\n') {
            return -1;
        } else {
            if (state->buflen >= MVTERM_ESCAPE_MAXLEN) return -1;
            state->buf[++state->buflen - 2] = c;
        }
    }
    return 0;
}

static ssize_t ringbuf_read_vterm (void* ctx, void* buf, size_t n) {
    return (ssize_t)vterm_output_read ((VTerm*)ctx, (char*)buf, n);
}
static ssize_t ringbuf_write_vterm (void* ctx, const void* buf, size_t n) {
    return (ssize_t)vterm_input_write ((VTerm*)ctx, (const char*)buf, n);
}

RINGBUF_READ_CALLBACK RINGBUF_READ_VTERM = ringbuf_read_vterm;
RINGBUF_WRITE_CALLBACK RINGBUF_WRITE_VTERM = ringbuf_write_vterm;
