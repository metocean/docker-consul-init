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
    fprintf(stderr, "[consul-init] " __VA_ARGS__); \
} while (0)

void print_help_and_exit(int exit_code) {
    PRINT("\n\n \
usage: consul-init --map [from-sig] [to-sig] --program [program path] [program args ..]\n\n \
--map [from-sig] [to-sig]: this re-maps a signal received by consul-init app to the program, you can have more than one mapping\n \
--program [norm program args]: this is the program + it args to be run in the docker\n\n \
--no-consul: do not use the consul agent\n\n \
example: /bin/consul-init --map TERM OUIT --program /bin/nginx -g daemon off;\n \
\n \
consul agent is started with:\n\n \
/usr/bin/consul agent -config-dir /etc/consul -data-dir /var/lib/consul/data\n \
\n \
Note these consul directories must exist.\n");
    exit(exit_code);
}

static struct {
    char **program_cmd;
    int signal_map[MAX_SIG_NAMES][2];
    int signal_map_len;
    bool no_consul;
} _args;

int map_signal(int signum) {
    for (int i = 0; i < _args.signal_map_len; i++)
        if (_args.signal_map[i][0] == signum)
            return _args.signal_map[i][1];
    return signum;
}

void parse_args(int argc, char** argv) {

    int sig_num = -1;

    enum {
        INIT_ARGS,
        GET_MAP_ARG_1,
        GET_MAP_ARG_2,
        GET_PROGRAM_ARG
    } state = INIT_ARGS;

    if (argc < 2)
      print_help_and_exit(1);

    memset(&_args, 0, sizeof(_args));

    for (int i = 1; i < argc; i++) {
        if (state == INIT_ARGS) {
            if (strcasecmp(argv[i], "--no-consul") == 0) {
              _args.no_consul = true;
            }
            else if (strcasecmp(argv[i], "--program") == 0
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

void check_for_consul_dirs(const char *consul_data_dir, const char *consul_config_dir) {
    if (_args.no_consul == false) {
        if (0 != access(consul_data_dir, F_OK)) {
            _args.no_consul = true;
            if (ENOENT == errno)
                PRINT("WARN: %s does not exists, consul agent will not be started", consul_data_dir);
            else if (ENOTDIR == errno)
                PRINT("WARN: %s is not a directory, consul agent will not be started", consul_data_dir);
            else
                PRINT("WARN: %s access error, consul agent will not be started", consul_data_dir);
        }
        else if (0 != access(consul_data_dir, F_OK)) {
            _args.no_consul = true;
            if (ENOENT == errno)
                PRINT("WARN: %s does not exists, consul agent will not be started", consul_config_dir);
            else if (ENOTDIR == errno)
                PRINT("WARN: %s is not a directory, consul agent will not be started", consul_config_dir);
            else
                PRINT("WARN: %s access error, consul agent will not be started", consul_config_dir);
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

    char* consul_data_dir = "/var/lib/consul/data";
    char* consul_config_dir = "/etc/consul";
    char* consul_cmd[] = {"/usr/bin/consul",
                          "agent",
                          "-config-dir", consul_config_dir,
                          "-data-dir", consul_data_dir,
                          NULL};

    parse_args(argc, argv);
    check_for_consul_dirs(consul_data_dir, consul_config_dir);

    pid_t program_pid = -1;
    pid_t program_exit_status = 0;
    pid_t program_alive = false;

    pid_t consul_pid = -1;
    pid_t consul_exit_status = 0;
    pid_t consul_alive = false;

    sigset_t all_signals;
    sigfillset(&all_signals);
    sigprocmask(SIG_BLOCK, &all_signals, NULL);

    program_pid = spawn_process(_args.program_cmd[0], _args.program_cmd, &all_signals);
    if (program_pid < 0)
        return program_pid;
    program_alive = true;

    if (!_args.no_consul) {
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
                    PRINT("%s (%d) exited with status %d.\n", _args.program_cmd[0], program_pid, exit_status);
                }
                else if (killed_pid == consul_pid) {
                    consul_alive = false;
                    consul_exit_status = exit_status;
                    PRINT("consul (%d) exited with status %d.\n", consul_pid, exit_status);
                }
                else {
                    PRINT("%s (%d) exited with status %d.\n", _args.program_cmd[0], killed_pid, exit_status);
                }
            }
        }
        else if (signum == SIGTERM || signum == SIGINT) {
            PRINT("starting graceful shutdown\n");
            if (consul_pid > 0 && consul_alive)
               kill(consul_pid, SIGINT);
            if (program_pid > 0 && program_alive)
                kill(program_pid, map_signal(SIGTERM));
        }
        else if (signum == SIGKILL) {
            PRINT("starting hard shutdown\n");
            if (program_pid > 0 && program_alive)
                kill(program_pid, SIGKILL);
            if (consul_pid > 0 && consul_alive)
                kill(consul_pid, SIGKILL);
        }
        else if (program_pid > 0 && program_alive) {
            PRINT("signalling %s (%d): %s\n",
                    _args.program_cmd[0],
                    program_pid,
                    strsignal(map_signal(signum)));
            kill(program_pid, map_signal(signum));
        }
    }

    if (program_exit_status || consul_exit_status) {
        PRINT("dirty exit: %s status(%d), consul status(%d)\n",
               _args.program_cmd[0],
               program_exit_status,
               consul_exit_status);
        return program_exit_status ? program_exit_status : consul_exit_status;
    } else {
        PRINT("clean exit\n");
        return 0;
    }
}
