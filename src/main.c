#include <errno.h>
#include <fcntl.h>
#include <libptytty.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <vterm.h>

#include "help_msg.h"
#include "mvterm.h"
#include "ringbuf.h"

uint64_t now_ms () {
    struct timespec ts;
    timespec_get (&ts, TIME_UTC);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}
int parse_uint (const char* str, uint64_t* val) {
    char* endptr;
    *val = strtoul (str, &endptr, 0);
    return !*endptr;
}
int parse_int (const char* str, int64_t* val) {
    char* endptr;
    *val = strtol (str, &endptr, 0);
    return !*endptr;
}
static void print_usage (FILE* out, const char* prog) {
    static const char token[] = "@PROG@";
    size_t token_len = sizeof (token) - 1;

    for (const char* cur = help_msg; *cur;) {
        if (strncmp (cur, token, token_len) == 0) {
            fputs (prog, out);
            cur += token_len;
        } else {
            fputc ((unsigned char)*cur, out);
            ++cur;
        }
    }
}

int main (int argc, char* const* argv) {
    uint64_t lines = 24, columns = 80;
    int visual_args = 0;
    uint64_t us = 10;
    uint64_t xms = 100;

    opterr = 0;
    for (int opt; (opt = getopt (argc, argv, ":c:l:s:x:vh")) != -1;) {
        if (opt == ':') {
            fprintf (stderr, "-%c: requires an argument\n", optopt);
            fprintf (stderr, "Try '%s -h' for more information.\n", argv[0]);
            return EXIT_FAILURE;
        } else if (opt == '?') {
            fprintf (stderr, "-%c: unknown option\n", optopt);
            fprintf (stderr, "Try '%s -h' for more information.\n", argv[0]);
            return EXIT_FAILURE;
        } else if (opt == 'c') {
            if (!parse_uint (optarg, &columns)) {
                fprintf (stderr, "-c: needs an uinteger\n");
                fprintf (stderr, "Try '%s -h' for more information.\n", argv[0]);
                return EXIT_FAILURE;
            }
        } else if (opt == 'l') {
            if (!parse_uint (optarg, &lines)) {
                fprintf (stderr, "-l: needs an uinteger\n");
                fprintf (stderr, "Try '%s -h' for more information.\n", argv[0]);
                return EXIT_FAILURE;
            }
        } else if (opt == 's') {
            if (!parse_uint (optarg, &us)) {
                fprintf (stderr, "-s: needs an uinteger\n");
                fprintf (stderr, "Try '%s -h' for more information.\n", argv[0]);
                return EXIT_FAILURE;
            }
        } else if (opt == 'v') {
            visual_args |= MVTERM_PRINT_VISUAL;
        } else if (opt == 'x') {
            if (!parse_uint (optarg, &xms)) {
                fprintf (stderr, "-x: needs an uinteger\n");
                fprintf (stderr, "Try '%s -h' for more information.\n", argv[0]);
                return EXIT_FAILURE;
            }
        } else if (opt == 'h') {
            print_usage (stdout, argv[0]);
            return EXIT_SUCCESS;
        }
    }

    VTerm* vt = vterm_new (lines, columns);
    if (!vt) {
        fprintf (stderr, "fail to create vterm\n");
        exit (EXIT_FAILURE);
    }
    vterm_set_utf8 (vt, 1);
    VTermScreen* vts = vterm_obtain_screen (vt);
    vterm_screen_reset (vts, 1);

    PTYTTY pty = ptytty_create ();
    if (!ptytty_get (pty)) {
        fprintf (stderr, "fail to get ptytty\n");
        exit (EXIT_FAILURE);
    }

    int master = ptytty_pty (pty);
    int slave = ptytty_tty (pty);
    fcntl (master, F_SETFL, fcntl (master, F_GETFL) | O_NONBLOCK);

    struct winsize ws;
    ws.ws_row = lines, ws.ws_col = columns, ws.ws_xpixel = ws.ws_ypixel = 0;
    if (ioctl (master, TIOCSWINSZ, &ws) == -1) {
        perror ("fail to set pty winsize");
        exit (EXIT_FAILURE);
    }

    pid_t pid = fork ();
    if (pid == 0) {
        setsid ();
        if (ioctl (slave, TIOCSCTTY, 0) == -1) {
            perror ("fail to set controlling tty");
            _exit (EXIT_FAILURE);
        }
        close (master);
        dup2 (slave, STDIN_FILENO);
        dup2 (slave, STDOUT_FILENO);
        dup2 (slave, STDERR_FILENO);
        if (slave > STDERR_FILENO) close (slave);
        setenv ("TERM", "xterm-256color", 1);
        argv[optind] ? execvp (argv[optind], argv + optind) : execlp ("sh", "sh", "-i", NULL);
        _exit (EXIT_FAILURE);
    }

    int pidfd = syscall (SYS_pidfd_open, pid, 0);
    if (pidfd == -1) {
        perror ("pidfd_open");
        exit (1);
    }

    close (slave);

    fcntl (STDIN_FILENO, F_SETFL, fcntl (STDIN_FILENO, F_GETFL) | O_NONBLOCK);
    fcntl (master, F_SETFL, fcntl (master, F_GETFL) | O_NONBLOCK);

    RINGBUF in_buf = ringbuf_alloc (RINGBUF_BLOCK), out_buf = ringbuf_alloc (RINGBUF_BLOCK);
    RINGBUF back_buf = ringbuf_alloc (RINGBUF_BLOCK);
    if (!in_buf || !out_buf || !back_buf) {
        ringbuf_free (in_buf), ringbuf_free (out_buf), ringbuf_free (back_buf);
        perror ("unable to allocate memory");
        exit (EXIT_FAILURE);
    }

    int status = VTERM_ESCAPE_INIT_STAT;
    uint64_t wait = now_ms ();
    while (1) {
        if (wait < now_ms ()) {
            int n;
            for (char c; (n = read (STDIN_FILENO, &c, 1)) > 0;) {
                int comm = vterm_escape_translate (in_buf, &status, c);
                if (comm == -1) {
                    fprintf (stderr, "unable to parse escape\n");
                    exit (EXIT_FAILURE);
                } else if (comm == MVTERM_COMM_PRINT) {
                    print_vterm (vt, visual_args);
                } else if (comm == MVTERM_COMM_PAUSE) {
                    wait = now_ms () + xms;
                    errno = EWOULDBLOCK;
                    break;
                }
            }
            if (n == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
                perror ("unable to read stdin");
                break;
            }
        }

        if (ringbuf_copyd_to (in_buf, &master, RINGBUF_WRITE_FD) < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            perror ("unable to write pty");
            break;
        }

        if (ringbuf_copyd_from (out_buf, &master, RINGBUF_READ_FD) < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            if (errno != EIO) perror ("unable to read pty");
            break;
        }

        int mov_byte = 0;
        mov_byte |= ringbuf_copyd_to (out_buf, vt, RINGBUF_WRITE_VTERM);
        mov_byte |= ringbuf_copyd_from (in_buf, vt, RINGBUF_READ_VTERM);

        if (!mov_byte) {
            if (us)
                usleep (us);
            else
                sched_yield ();
        }
    }

    close (master);
    vterm_free (vt);

    syscall (SYS_pidfd_send_signal, pidfd, SIGKILL, NULL, 0);
    waitpid (pid, NULL, 0);

    close (pidfd);
    ptytty_delete (pty);

    return 0;
}
