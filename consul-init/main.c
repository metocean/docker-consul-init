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
    fprintf(stderr, __VA_ARGS__); \
} while (0)

void print_args(int argc, char** argv) {
    int i;
    PRINT("entrypoint args: ");
    for (i = 0; i<argc; i++)
        PRINT("\"%s\" ", argv[i]);
    PRINT("\n");
}

void print_help_and_exit(int exit_code) {
    PRINT("\n\n\
usage: consul-init --map [from-sig] [to-sig] --init [program / args ..] --program [program / args ..]\n\n \
--map [from-sig] [to-sig]: this re-maps a signal received by consul-init app to the program, you can have more than one mapping\n\n \
--shutdown [sig]: consul-init will try a greaceful shutdown when receiving this signal or INT or TERM\n\n \
--program [norm program args]: this is the program + it args to be run in the docker\n\n \
--init [init program args]: the init program runs first, before consul and --program. If it returns nonzero consul-init will exit. \n\n \
--no-consul: do not use the consul agent\n\n \
example: consul-init --map TERM QUIT --program /bin/nginx -g daemon off;\n \
example: consul-init --map TERM QUIT --init wget http://[somesite]/config.json --program /bin/nginx -g daemon off;\n \
\n \
consul agent is started with:\n\n \
/usr/bin/consul agent -config-dir /consul/config -data-dir /consul/data\n \
\n \
Note these consul directories must exist or the consul agent will not start.\n\n");
    exit(exit_code);
}

#define MAX_ARGS 50
static struct {
    char *init_cmd[MAX_ARGS + 1];
    char *program_cmd[MAX_ARGS + 1];
    int signal_map[MAX_SIG_NAMES][2];
    int signal_map_len;
    bool no_consul;
    int shutdown_sig;
} _args;

int map_signal(int signum) {
    int i = 0;
    for (; i < _args.signal_map_len; i++)
        if (_args.signal_map[i][0] == signum)
            return _args.signal_map[i][1];
    return signum;
}

void parse_args(int argc, char** argv) {

    enum {
        INIT_ARGS,
        GET_MAP_ARG_1,
        GET_MAP_ARG_2,
        GET_SHUTDOWN_SIG,
        GET_INIT_ARG,
        GET_INIT_ARG_COUNT,
        GET_PROGRAM_ARG,
        GET_PROGRAM_ARG_COUNT
    } state = INIT_ARGS;

    print_args(argc, argv);

    memset(&_args, 0, sizeof(_args));

    int sig_num = -1;

    char **init_cmd = NULL;
    int init_cmd_n = 0;

    char **program_cmd = NULL;
    int program_cmd_n = 0;

    int i = 1;
    _args.shutdown_sig = -1;

    for (; i < argc; i++) {

        if (strcasecmp(argv[i], "--help") == 0
                || strcasecmp(argv[i], "-h") == 0) {
            print_help_and_exit(0);
        }
        else if (strcasecmp(argv[i], "--init") == 0) {
            state = GET_INIT_ARG;
        }
        else if (strcasecmp(argv[i], "--program") == 0) {
            state = GET_PROGRAM_ARG;
        }
        else if (strcasecmp(argv[i], "--no-consul") == 0) {
            state = INIT_ARGS;
            _args.no_consul = true;
        }
        else if (state == INIT_ARGS) {

            if (strcasecmp(argv[i], "--map") == 0) {
                state = GET_MAP_ARG_1;
                if (_args.signal_map_len == MAX_SIG_NAMES) {
                    PRINT("ERROR: to many signals mapped, max: %d\n",
                            MAX_SIG_NAMES);
                    print_help_and_exit(1);
                }
            }
            else if (strcasecmp(argv[i], "--shutdown") == 0) {
                state = GET_SHUTDOWN_SIG;
            }
            else if (strcasecmp(argv[i], "--help") == 0
                    || strcasecmp(argv[i], "-h") == 0) {
                print_help_and_exit(0);
            }
            else {
                PRINT("ERROR: invalid arguments\n");
                print_help_and_exit(1);
            }

          } else if (state == GET_SHUTDOWN_SIG) {
              if ((sig_num = sig_from_str(argv[i])) < 1) {
                  PRINT("ERROR: invalid --shutdown signal, valid signals are:\n");
                  print_sigs();
                  print_help_and_exit(1);
              }
              _args.shutdown_sig = sig_num;
              state = INIT_ARGS;

        } else if (state == GET_MAP_ARG_1) {
            if ((sig_num = sig_from_str(argv[i])) < 1) {
                PRINT("ERROR: invalid [from-sig], valid signals are:\n");
                print_sigs();
                print_help_and_exit(1);
            }
            _args.signal_map[_args.signal_map_len][0] = sig_num;
            state = GET_MAP_ARG_2;

        } else if (state == GET_MAP_ARG_2) {
            if ((sig_num = sig_from_str(argv[i])) < 1) {
                PRINT("ERROR: invalid [to-sig], valid signals are:\n");
                print_sigs();
                print_help_and_exit(1);
            }
            _args.signal_map[_args.signal_map_len++][1] = sig_num;
            state = INIT_ARGS;

        } else if (state == GET_INIT_ARG) {
            init_cmd = &argv[i];
            init_cmd_n++;
            state = GET_INIT_ARG_COUNT;

        } else if (state == GET_INIT_ARG_COUNT) {
            init_cmd_n++;

        } else if (state == GET_PROGRAM_ARG) {
            program_cmd = &argv[i];
            program_cmd_n++;
            state = GET_PROGRAM_ARG_COUNT;

        } else if (state == GET_PROGRAM_ARG_COUNT) {
            program_cmd_n++;
        }
    }

    if(init_cmd_n) {
        for (i = 0; i < init_cmd_n && i < MAX_ARGS; i++)
            _args.init_cmd[i] = init_cmd[i];
    }

    if(program_cmd_n) {
        for (i = 0; i < program_cmd_n && i < MAX_ARGS; i++)
            _args.program_cmd[i] = program_cmd[i];
    }
}


pid_t spawn_cmd(const char *file,
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

int execute_cmd(char **argv) {
    pid_t  pid;
    int    status;

    if ((pid = fork()) < 0) {     /* fork a child process           */
        printf("*** ERROR: forking child process failed\n");
        exit(1);
    }
    else if (pid == 0) {
        if (execvp(*argv, argv) < 0)
             exit(1);
    }
    else {
        while (wait(&status) != pid);
    }
    return status;
}

char * find_dir(char *dir1, char *dir2) {
    if (access(dir1, F_OK) == 0) {
        return dir1;
    } else if (access(dir2, F_OK) == 0) {
        return dir2;
    }
    return NULL;
}

void kill_app(int pid, int sig) {
    PRINT("sending sig: %s(%d) to pid:%d\n", strsignal(sig), sig, pid);
    kill(pid, sig);
}

int main(int argc, char** argv) {

    parse_args(argc, argv);

    char* consul_cmd[] = {
        "/usr/bin/consul",
        "agent",
        "-data-dir", find_dir("/consul/data", "/var/lib/consul/data"),
        "-config-dir", find_dir("/consul/config", "/etc/consul"),
        NULL
    };

    if (!_args.no_consul) {
        if (consul_cmd[3] == NULL) {
            _args.no_consul = true;
            PRINT("WARN: could not access config dir, consul agent will not be started\n");
        }
        if (consul_cmd[5] == NULL) {
            _args.no_consul = true;
            PRINT("WARN: could not access data dir, consul agent will not be started\n");
        }
    }

    if (_args.init_cmd[0] && execute_cmd(_args.init_cmd) != 0) {
        PRINT("ERROR: calling init cmd '%s' failed. Exiting.\n",
              _args.init_cmd[0]);
        exit(2);
    }

    pid_t program_pid = -1;
    pid_t program_exit_status = 0;
    pid_t program_alive = false;

    pid_t consul_pid = -1;
    pid_t consul_exit_status = 0;
    pid_t consul_alive = false;
    pid_t consul_closing = false;

    sigset_t all_signals;
    sigfillset(&all_signals);
    sigprocmask(SIG_BLOCK, &all_signals, NULL);

    program_pid = spawn_cmd(_args.program_cmd[0], _args.program_cmd, &all_signals);
    if (program_pid < 0)
        return program_pid;
    program_alive = true;

    if (!_args.no_consul) {
      consul_pid = spawn_cmd(consul_cmd[0], consul_cmd, &all_signals);
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

                    if (consul_pid != -1 && consul_alive && !consul_closing) {
                        PRINT("signalling consul pid:%d\n", consul_pid);
                        kill_app(consul_pid, SIGINT);
                        consul_closing = true;
                    }
                }
                else if (killed_pid == consul_pid) {
                    consul_alive = false;
                    consul_exit_status = exit_status;
                    PRINT("consul (%d) exited with status %d.\n", consul_pid, exit_status);
                }
                else {
                    PRINT("pid:%d exited with status %d.\n", killed_pid, exit_status);
                }
            }
        }
        else if (signum == SIGTERM || signum == SIGINT
          || (_args.shutdown_sig != -1 && signum == _args.shutdown_sig)) {

            PRINT("starting graceful shutdown\n");

            if (consul_pid != -1 && consul_alive) {
                  PRINT("signalling consul pid:%d\n", consul_pid);
                  kill_app(consul_pid, SIGINT);
                  consul_closing = true;
            }

            if (program_pid != -1 && program_alive) {
                PRINT("signalling %s pid:%d\n", _args.program_cmd[0], program_pid);
                kill_app(program_pid, map_signal(signum));
            }
        }
        else if (signum == SIGKILL) {
            PRINT("starting hard shutdown\n");

            if (program_pid != -1 && program_alive) {
                PRINT("signalling %s (%d)\n", _args.program_cmd[0], program_pid);
                kill_app(program_pid, SIGKILL);
            }

            if (consul_pid != -1 && consul_alive) {
                PRINT("signalling consul pid:%d\n", consul_pid);
                kill_app(consul_pid, SIGKILL);
                consul_closing = true;
              }
        }
        else if (program_pid != -1 && program_alive) {
            PRINT("signalling %s pid:%d\n", _args.program_cmd[0], program_pid);
            kill_app(program_pid, map_signal(signum));
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
