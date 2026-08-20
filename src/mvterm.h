#ifndef __MVTERM_H__
#define __MVTERM_H__

#include <vterm.h>

#include "ringbuf.h"

void print_vterm (VTerm* vt, int args);

#define MVTERM_PRINT_VISUAL 1
#define MVTERM_PRINT_PRETTY 2

extern const int VTERM_ESCAPE_INIT_STAT;
int vterm_escape_translate (RINGBUF dest, int* status, char c);

#define MVTERM_COMM_PRINT 1
#define MVTERM_COMM_PAUSE 2

extern RINGBUF_READ_CALLBACK RINGBUF_READ_VTERM;
extern RINGBUF_WRITE_CALLBACK RINGBUF_WRITE_VTERM;

#endif
