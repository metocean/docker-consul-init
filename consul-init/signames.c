#include <signal.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include "signames.h"


struct signal_name {
    int num;
    const char *name;
    const char *description;
};

static struct signal_name _signals[MAX_SIG_NAMES];
static int _signals_len = 0;

static void add_signal(int num, const char *name, const char *description) {
    _signals[_signals_len].num = num;
    _signals[_signals_len].name = name;
    _signals[_signals_len++].description = description;
}

static bool str_starts_with(const char *str, const char *prefix)
{
    size_t lenpre = strlen(prefix),
           lenstr = strlen(str);
    return lenstr < lenpre ? false : strncmp(prefix, str, lenpre) == 0;
}

static void signal_names_init() {
    memset(&_signals, 0, sizeof(_signals));

    // sigals use by consul-init so required.
    add_signal(SIGINT, "INT", "Interrupt (ANSI)");
    add_signal(SIGTERM, "TERM", "Termination (ANSI).");
    add_signal(SIGCHLD, "CHLD", "Child status has changed (POSIX).");
    add_signal(SIGUNUSED, "UNUSED", "");

    #ifdef SIGHUP
        add_signal(SIGHUP, "HUP", "Quit (POSIX)");
    #endif
    #ifdef SIGQUIT
        add_signal(SIGQUIT, "QUIT", "Quit (POSIX)");
    #endif
    #ifdef SIGILL
        add_signal(SIGILL, "ILL", "Illegal instruction (ANSI).");
    #endif
    #ifdef SIGTRAP
        add_signal(SIGTRAP, "TRAP", "Trace trap (POSIX).");
    #endif
    #ifdef SIGABRT
        add_signal(SIGABRT, "ABRT", "Abort (ANSI).");
    #endif
    #ifdef SIGIOT
        add_signal(SIGIOT, "IOT", "IOT (4.2 BSD).");
    #endif
    #ifdef SIGBUS
        add_signal(SIGBUS, "BUS", "BUS error (4.2 BSD).");
    #endif
    #ifdef SIGFPE
        add_signal(SIGFPE, "FPE", "Floating-point exception (ANSI).");
    #endif
    #ifdef SIGKILL
        add_signal(SIGKILL, "KILL", "Kill, unblockable (POSIX).");
    #endif
    #ifdef SIGUSR1
        add_signal(SIGUSR1, "USR1", "User-defined signal 1 (POSIX).");
    #endif
    #ifdef SIGSEGV
        add_signal(SIGSEGV, "SEGV", "Segmentation violation (ANSI).");
    #endif
    #ifdef SIGUSR2
        add_signal(SIGUSR2, "USR2", "User-defined signal 2 (POSIX).");
    #endif
    #ifdef SIGPIPE
        add_signal(SIGPIPE, "PIPE", "Broken pipe (POSIX).");
    #endif
    #ifdef SIGALRM
        add_signal(SIGALRM, "ALRM", "Alarm clock (POSIX).");
    #endif
    #ifdef SIGSTKFLT
        add_signal(SIGSTKFLT, "STKFLT", "Stack fault.");
    #endif
    #ifdef SIGCLD
        add_signal(SIGCLD, "CLD", "Same as SIGCHLD (System V).");
    #endif
    #ifdef SIGCONT
        add_signal(SIGCONT, "CONT", "Continue (POSIX).");
    #endif
    #ifdef SIGSTOP
        add_signal(SIGSTOP, "STOP", "Stop, unblockable (POSIX).");
    #endif
    #ifdef SIGTSTP
        add_signal(SIGTSTP, "STP", "Keyboard stop (POSIX).");
    #endif
    #ifdef SIGTTIN
        add_signal(SIGTTIN, "TTIN", "Background read from tty (POSIX).");
    #endif
    #ifdef SIGTTOU
        add_signal(SIGTTOU, "TTOU", "Background write to tty (POSIX).");
    #endif
    #ifdef SIGURG
        add_signal(SIGURG, "URG", "Urgent condition on socket (4.2 BSD).");
    #endif
    #ifdef SIGXCPU
        add_signal(SIGXCPU, "XCPU", "CPU limit exceeded (4.2 BSD).");
    #endif
    #ifdef SIGXFSZ
        add_signal(SIGXFSZ, "XFSZ", "File size limit exceeded (4.2 BSD).");
    #endif
    #ifdef SIGVTALRM
        add_signal(SIGVTALRM, "VTALRM", "Virtual alarm clock (4.2 BSD).");
    #endif
    #ifdef SIGPROF
        add_signal(SIGPROF, "PROF", "Profiling alarm clock (4.2 BSD).");
    #endif
    #ifdef SIGWINCH
        add_signal(SIGWINCH, "WINCH", " Window size change (4.3 BSD, Sun).");
    #endif
    #ifdef SIGPOLL
        add_signal(SIGPOLL, "POLL", "Pollable event occurred (System V).");
    #endif
    #ifdef SIGIO
        add_signal(SIGIO, "IO", "I/O now possible (4.2 BSD).");
    #endif
    #ifdef SIGPWR
        add_signal(SIGPWR, "PWR", "Power failure restart (System V).");
    #endif
    #ifdef SIGSYS
        add_signal(SIGSYS, "SYS", "Bad system call.");
    #endif
}

int signal_name_to_num(const char* name) {
    if (str_starts_with(name, "SIG"))
        name = &name[3];
    int i = 0;
    for (; i < _signals_len; i++)
        if (strcasecmp(_signals[i].name, name) == 0)
            return _signals[i].num;
    return -1;
}

int sig_from_str(const char *str) {
    if (_signals_len == 0)
        signal_names_init();

    int num = atoi(str);
    if (num > 0) {
        int i = 0;
        for (; i < _signals_len; i++)
            if (_signals[i].num == num)
                return num;
        return -1;
    }
    // it might be a name / code.
    return signal_name_to_num(str);
}
