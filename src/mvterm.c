#include "mvterm.h"

#include <stdio.h>
#include <time.h>
#include <vterm.h>

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
        putchar ('\x1b');
        putchar ('[');
        if (id > 0) {
            if (id >= 10) putchar (id / 10 + '0');
            putchar (id % 10 + '0');
        }
        putchar (type);
    } else {
        putchar ('%');
        putchar ('c');
        if (id > 0) {
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
    if (VTERM_COLOR_IS_RGB (clr)) {
        if (args & MVTERM_PRINT_VISUAL) {
            putchar ('\x1b');
            putchar ('[');
            putchar (isfg ? '3' : '4');
            putchar ('8');
            putchar (';');
            putchar ('2');
            putchar (';');
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
            putchar ('\x1b');
            putchar ('[');
            putchar (isfg ? '3' : '4');
            putchar ('8');
            putchar (';');
            putchar ('5');
            putchar (';');
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
void print_vterm (VTerm* vt, int args) {
    VTermState* vtst = vterm_obtain_state (vt);

    int rows, cols;
    vterm_get_size (vt, &rows, &cols);

    VTermColor fg, bg;
    vterm_state_get_default_colors (vtst, &fg, &bg);

    VTermPos curp;
    vterm_state_get_cursorpos (vtst, &curp);

    if (args & MVTERM_PRINT_VISUAL) {
        print_color (&fg, 1, args);
        print_color (&bg, 0, args);
    } else {
        print_color (&fg, 1, args);
        print_color (&bg, 0, args);

        struct timespec now;
        clock_gettime (CLOCK_REALTIME, &now);

        char buf[64];
        snprintf (buf, sizeof (buf), " %ld.%09ld %d;%d\n", now.tv_sec, now.tv_nsec, curp.row, curp.col);
        for (char* c = buf; *c; ++c) putchar (*c);
    }

    VTermScreenCellAttrs attr = {};
    VTermColor cfg = fg, cbg = bg;

    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols;) {
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

            if (cell.width) {
                if (!cell.chars[0]) {
                    if (!(args & MVTERM_PRINT_VISUAL)) {
                        putchar ('%');
                        putchar (' ');
                    } else {
                        putcsi (-1, 'C', args);
                    }
                } else {
                    if (!(args & MVTERM_PRINT_VISUAL)) {
                        if (cell.width > 1 || cell.chars[1]) {
                            putchar ('%');
                            putchar (cell.width + '0');
                            int cpw = 0;
                            for (; cpw < VTERM_MAX_CHARS_PER_CELL && cell.chars[cpw]; cpw++) continue;
                            putchar (cpw + '0');
                        }
                    }

                    for (int i = 0; i < VTERM_MAX_CHARS_PER_CELL && cell.chars[i]; ++i) {
                        uint32_t cp = cell.chars[i];
                        if (!(args & MVTERM_PRINT_VISUAL)) {
                            if (cp == '%') putchar ('%');
                        }
                        pututf8 (cp);
                    }
                }

                c += cell.width;
            } else {
                fprintf (stderr, "vterm screen format error\n");
                ++c;
            }
        }

        putchar ('\n');
    }

    if (args & MVTERM_PRINT_VISUAL) putcsi (0, 'm', args);
}

static int vterm_escape (RINGBUF dest, int escape) {
    if (escape == 48) {  // <L>
        ringbuf_writed (dest, "<", 1);
    } else if (escape == 52) {  // <P>
        return MVTERM_COMM_PRINT;
    } else if (escape == 60) {  // <X>
        return MVTERM_COMM_PAUSE;
    } else if (escape == 2550) {  // <CR>
        ringbuf_writed (dest, "\r", 1);
    } else if (escape == 171495) {  // <ESC>
        ringbuf_writed (dest, "\x1b", 1);
    } else {
        fprintf (stderr, "unknown escape %d\n", escape);
        return -1;
    }

    return 0;
}

const int VTERM_ESCAPE_INIT_STAT = -1;
int vterm_escape_translate (RINGBUF dest, int* status, char c) {
    if (*status == -1) {
        if (c == '<')
            *status = 0;
        else if (c != '\n' && c != '\r')
            ringbuf_writed (dest, &c, 1);

    } else {
        if (c == '>') {
            int ret = vterm_escape (dest, *status);
            *status = -1;
            return ret;
        }
        if (*status & (63 << 24)) return -1;

        if ('0' <= c && c <= '9') {
            *status = *status * 64 + c - '0' + 1;
        } else if ('a' <= c && c <= 'z') {
            *status = *status * 64 + c - 'a' + 11;
        } else if ('A' <= c && c <= 'Z') {
            *status = *status * 64 + c - 'A' + 37;
        } else {
            return -1;
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
