#ifndef __MVTERM_H__
#define __MVTERM_H__

#include <vterm.h>

#include "ringbuf.h"

#define MVTERM_ESCAPE_MAXLEN 128

typedef struct {
    char buf[MVTERM_ESCAPE_MAXLEN];
    size_t buflen;
} VTERM_STATE;
int mvterm_escape_translate (RINGBUF dest, VTERM_STATE* state, char c, VTerm* vt);

#define MVTERM_COMM_RESIZE 1
#define MVTERM_COMM_PAUSE 2
#define MVTERM_COMM_END 3

extern RINGBUF_READ_CALLBACK RINGBUF_READ_VTERM;
extern RINGBUF_WRITE_CALLBACK RINGBUF_WRITE_VTERM;

#endif
