#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "signames.h"

#define PRINT(...) do { \
    fprintf(stderr, "[consul-nginx-init] " __VA_ARGS__); \
} while (0)

void print_help_and_exit(int exit_code) {
    PRINT("\n\n \
usage: consul-init --map [from-sig] [to-sig] --program [program path] [program args ..]\n\n \
--map [from-sig] [to-sig]: this re-maps a signal received by consul-init app to the program\n \
--program [norm program args]: this is the program + it args to be run in the docker\n\n \
example: /bin/consul-init --map INT HUP --map 12 13 --program /bin/nginx -g daemon off;\n");
    exit(exit_code);
}

static struct {
    char **program_cmd;
    int signal_map[MAX_SIG_NAMES][2];
    int signal_map_len;
} _args;

int map_signal(int signum) {
    for (int i = 0; i < _args.signal_map_len; i++)
        if (_args.signal_map[i][0] == signum)
            return _args.signal_map[i][1];
    return signum;
}

void parse_args(int argc, char** argv) {

    enum {
        INIT_ARGS,
        GET_MAP_ARG_1,
        GET_MAP_ARG_2,
        GET_PROGRAM_ARG
    } state = INIT_ARGS;

    memset(&_args, 0, sizeof(_args));

    int sig_num = -1;

    for (int i = 1; i < argc; i++) {
        if (state == INIT_ARGS) {
            if (strcasecmp(argv[i], "--program") == 0
                    || strcasecmp(argv[i], "-p") == 0) {
                state = GET_PROGRAM_ARG;
            }
            else if (strcasecmp(argv[i], "--map") == 0
                    || strcasecmp(argv[i], "-m") == 0) {
                state = GET_MAP_ARG_1;
                if (_args.signal_map_len == MAX_SIG_NAMES) {
                    PRINT("ERROR: to many signals mapped, max: %d\n",
                            MAX_SIG_NAMES);
                    print_help_and_exit(1);
                }
            }
            else if (strcasecmp(argv[i], "--help") == 0
                    || strcasecmp(argv[i], "-h") == 0) {
                print_help_and_exit(0);
            }
            else {
                PRINT("ERROR: invalid arguments\n");
                print_help_and_exit(1);
            }
        }
        else if (state == GET_MAP_ARG_1) {
            if ((sig_num = sig_from_str(argv[i])) < 1) {
                PRINT("ERROR: invalid from signal\n");
                print_help_and_exit(1);
            }
            _args.signal_map[_args.signal_map_len][0] = sig_num;
            state = GET_MAP_ARG_2;
        }
        else if (state == GET_MAP_ARG_2) {
            if ((sig_num = sig_from_str(argv[i])) < 1) {
                PRINT("ERROR: invalid to signal\n");
                print_help_and_exit(1);
            }
            _args.signal_map[_args.signal_map_len++][1] = sig_num;
            state = INIT_ARGS;
        }
        else if (state == GET_PROGRAM_ARG) {
            _args.program_cmd = &argv[i];
            break;
        }
    }
}

pid_t spawn_process(const char *file,
                    char *const argv[],
                    const sigset_t *all_signals) {

    pid_t child_pid = fork();
    if (child_pid < 0) {
        PRINT("ERROR: Unable to fork %s. Exiting.\n", file);
        return child_pid;
    }
    else if (child_pid == 0) {
        sigprocmask(SIG_UNBLOCK, all_signals, NULL);
        if (setsid() == -1) {
            PRINT(
                "ERROR: Unable to setsid (errno=%d %s) for %s. Exiting.\n",
                errno,
                strerror(errno),
                file
            );
            exit(1);
        }
        execvp(file, argv);
        // if this point is reached, exec failed, so we should exit nonzero
        PRINT("ERROR: %s: %s\n", file, strerror(errno));
        return 2;
    }
    return child_pid;
}


int main(int argc, char** argv) {

    char* consul_cmd[] = {"/usr/bin/consul",
                          "agent", "-config-dir", "/etc/consul",
                          "-data-dir", "/var/lib/consul/data", NULL};

    parse_args(argc, argv);

    pid_t program_pid = -1;
    pid_t program_exit_status = 0;
    pid_t program_alive = false;

    pid_t consul_pid = -1;
    pid_t consul_exit_status = 0;
    pid_t consul_alive = false;

    bool no_consul = argc > 1 && strcmp("--no-consul", argv[1]) == 0;

    sigset_t all_signals;
    sigfillset(&all_signals);
    sigprocmask(SIG_BLOCK, &all_signals, NULL);

    program_pid = spawn_process(_args.program_cmd[0], _args.program_cmd, &all_signals);
    if (program_pid < 0)
        return program_pid;
    program_alive = true;

    if (!no_consul) {
      consul_pid = spawn_process(consul_cmd[0], consul_cmd, &all_signals);
      if (consul_pid < 0)
          return consul_pid;
      consul_alive = true;
    }

    while (program_alive || consul_alive) {
        int signum;
        sigwait(&all_signals, &signum);
        PRINT("Received signal(%d) '%s'.\n", signum, strsignal(signum));
        if (signum == SIGCHLD) {
            int status, exit_status;
            pid_t killed_pid;
            while ((killed_pid = waitpid(-1, &status, WNOHANG)) > 0) {
                if (WIFEXITED(status)) {
                    exit_status = WEXITSTATUS(status);
                }
                else {
                    assert(WIFSIGNALED(status));
                    exit_status = 128 + WTERMSIG(status);
                }

                if (killed_pid == program_pid) {
                    program_alive = false;
                    program_exit_status = exit_status;
                    PRINT("nginx(%d) exited with status %d.\n", program_pid, exit_status);
                }
                else if (killed_pid == consul_pid) {
                    consul_alive = false;
                    consul_exit_status = exit_status;
                    PRINT("consul(%d) exited with status %d.\n", consul_pid, exit_status);
                }
                else {
                    PRINT("process(%d) exited with status %d.\n", killed_pid, exit_status);
                }
            }
        }
        else if (signum == SIGTERM) {
            PRINT("starting graceful shutdown");
            if (consul_pid > 0)
               kill(consul_pid, SIGINT);
            if (program_pid > 0)
                kill(program_pid, map_signal(SIGTERM));
        }
        else if (signum == SIGKILL) {
            PRINT("starting hard shutdown");
            if (program_pid > 0)
                kill(program_pid, SIGKILL);
            if (consul_pid > 0)
                kill(consul_pid, SIGKILL);
        }
        else if (program_pid > 0) {
            PRINT("signalling nginx(%d): %s",
                    program_pid,
                    strsignal(map_signal(signum)));
            kill(program_pid, map_signal(signum));
        }
    }

    if (program_exit_status || consul_exit_status) {
        PRINT("dirty exit: nginx status(%d), consul status(%d) \n",
               program_exit_status,
               consul_exit_status);
        return program_exit_status ? program_exit_status : consul_exit_status;
    } else {
        PRINT("clean exit\n");
        return 0;
    }
}
