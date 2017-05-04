#include <sys/signal.h>
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
    add_signal(SIGHUP, "HUP", "Hangup (POSIX)");
    add_signal(SIGINT, "INT", "Interrupt (ANSI)");
    add_signal(SIGQUIT, "QUIT", "Quit (POSIX)");
    add_signal(SIGILL, "ILL", "Illegal instruction (ANSI).");
    add_signal(SIGILL, "TRAP", "Trace trap (POSIX).");
    add_signal(SIGABRT, "ABRT", "Abort (ANSI).");
    add_signal(SIGIOT, "IOT", "IOT (4.2 BSD).");
    add_signal(SIGBUS, "BUS", "BUS error (4.2 BSD).");
    add_signal(SIGFPE, "FPE", "Floating-point exception (ANSI).");
    add_signal(SIGKILL, "KILL", "Kill, unblockable (POSIX).");
    add_signal(SIGUSR1, "USR1", "User-defined signal 1 (POSIX).");
    add_signal(SIGSEGV, "SEGV", "Segmentation violation (ANSI).");
    add_signal(SIGUSR2, "USR2", "User-defined signal 2 (POSIX).");
    add_signal(SIGPIPE, "PIPE", "Broken pipe (POSIX).");
    add_signal(SIGALRM, "ALRM", "Alarm clock (POSIX).");
    add_signal(SIGTERM, "TERM", "Termination (ANSI).");
    add_signal(SIGSTKFLT, "STKFLT", "Stack fault.");
    add_signal(SIGCLD, "CLD", "Child status has changed (System V).");
    add_signal(SIGCHLD, "CHLD", "Child status has changed (POSIX).");
    add_signal(SIGCONT, "CONT", "Continue (POSIX).");
    add_signal(SIGSTOP, "STOP", "Stop, unblockable (POSIX).");
    add_signal(SIGTSTP, "STP", "Keyboard stop (POSIX).");
    add_signal(SIGTTIN, "TTIN", "Background read from tty (POSIX).");
    add_signal(SIGTTOU, "TTOU", "Background write to tty (POSIX).");
    add_signal(SIGURG, "URG", "Urgent condition on socket (4.2 BSD).");
    add_signal(SIGXCPU, "XCPU", "CPU limit exceeded (4.2 BSD).");
    add_signal(SIGXFSZ, "XFSZ", "File size limit exceeded (4.2 BSD).");
    add_signal(SIGVTALRM, "VTALRM", "Virtual alarm clock (4.2 BSD).");
    add_signal(SIGPROF, "PROF", "Profiling alarm clock (4.2 BSD).");
    add_signal(SIGWINCH, "WINCH", " Window size change (4.3 BSD, Sun).");
    add_signal(SIGPOLL, "POLL", "Pollable event occurred (System V).");
    add_signal(SIGIO, "IO", "I/O now possible (4.2 BSD).");
    add_signal(SIGPWR, "PWR", "Power failure restart (System V).");
    add_signal(SIGSYS, "SYS", "Bad system call.");
    add_signal(SIGUNUSED, "UNUSED", "");
}

int signal_name_to_num(const char* name) {
    if (str_starts_with(name, "SIG"))
        name = &name[3];

    for (int i = 0; i < _signals_len; i++)
        if (strcasecmp(_signals[i].name, name) == 0)
            return _signals[i].num;
    return -1;
}

int sig_from_str(const char *str) {
    if (_signals_len == 0)
        signal_names_init();

    int num = atoi(str);
    if (num > 0) {
        for (int i = 0; i < _signals_len; i++)
            if (_signals[i].num == num)
                return num;
        return -1;
    }
    // it might be a name / code.
    return signal_name_to_num(str);
}
