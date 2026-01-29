#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>

#define INPUT_LENGTH 2048
#define MAX_ARGS 512
#define MAX_BG_PROCS 100

struct command_line {
    char *argv[MAX_ARGS + 1];
    int argc;
    char *input_file;
    char *output_file;
    bool is_bg;
};

int last_fg_status = 0;
int bg_allowed = 1;
pid_t fg_pid = 0;
pid_t bg_pids[MAX_BG_PROCS];
int bg_count = 0;

void free_command(struct command_line *cmd) {
    for (int i = 0; i < cmd->argc; i++) {
        free(cmd->argv[i]);
    }
    if (cmd->input_file) free(cmd->input_file);
    if (cmd->output_file) free(cmd->output_file);
    free(cmd);
}

void handle_sigint(int signo) {
    if (fg_pid > 0) {
        kill(fg_pid, SIGINT);
    }
}

void handle_sigtstp(int signo) {
    char *msg_on = "\nEntering foreground-only mode (& is now ignored)\n: ";
    char *msg_off = "\nExiting foreground-only mode\n: ";
    if (bg_allowed) {
        write(STDOUT_FILENO, msg_on, strlen(msg_on));
        bg_allowed = 0;
    } else {
        write(STDOUT_FILENO, msg_off, strlen(msg_off));
        bg_allowed = 1;
    }
}

struct command_line *parse_input() {
    char input[INPUT_LENGTH];
    struct command_line *curr_command = (struct command_line *)calloc(1, sizeof(struct command_line));

    printf(": ");
    fflush(stdout);
    if (fgets(input, INPUT_LENGTH, stdin) == NULL) {
        curr_command->argc = 0;
        return curr_command;
    }

    if (input[0] == '\n' || input[0] == '#') {
        curr_command->argc = 0;
        return curr_command;
    }

    char *token = strtok(input, " \n");
    while (token) {
        if (!strcmp(token, "<")) {
            token = strtok(NULL, " \n");
            if (token) curr_command->input_file = strdup(token);
        } else if (!strcmp(token, ">")) {
            token = strtok(NULL, " \n");
            if (token) curr_command->output_file = strdup(token);
        } else if (!strcmp(token, "&") && strtok(NULL, " \n") == NULL) {
            curr_command->is_bg = true;
        } else {
            curr_command->argv[curr_command->argc++] = strdup(token);
        }
        token = strtok(NULL, " \n");
    }
    curr_command->argv[curr_command->argc] = NULL;
    return curr_command;
}

int run_builtin(struct command_line *cmd) {
    if (cmd->argc == 0) return 1;

    if (strcmp(cmd->argv[0], "exit") == 0) {
        for (int i = 0; i < bg_count; i++) {
            kill(bg_pids[i], SIGTERM);
        }
        exit(0);
    } else if (strcmp(cmd->argv[0], "cd") == 0) {
        if (cmd->argc == 1) {
            chdir(getenv("HOME"));
        } else {
            chdir(cmd->argv[1]);
        }
        return 1;
    } else if (strcmp(cmd->argv[0], "status") == 0) {
        if (WIFEXITED(last_fg_status)) {
            printf("exit value %d\n", WEXITSTATUS(last_fg_status));
        } else if (WIFSIGNALED(last_fg_status)) {
            printf("terminated by signal %d\n", WTERMSIG(last_fg_status));
        }
        fflush(stdout);
        return 1;
    }
    return 0;
}

void execute_command(struct command_line *cmd) {
    pid_t pid = fork();
    if (pid == 0) { // Child process
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_IGN);

        if (cmd->input_file) {
            int fd = open(cmd->input_file, O_RDONLY);
            if (fd < 0) {
                printf("cannot open %s for input\n", cmd->input_file);
                fflush(stdout);
                exit(1);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
        } else if (cmd->is_bg && bg_allowed) {
            int fd = open("/dev/null", O_RDONLY);
            dup2(fd, STDIN_FILENO);
            close(fd);
        }

        if (cmd->output_file) {
            int fd = open(cmd->output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) {
                printf("cannot open %s for output\n", cmd->output_file);
                fflush(stdout);
                exit(1);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        } else if (cmd->is_bg && bg_allowed) {
            int fd = open("/dev/null", O_WRONLY);
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }

        execvp(cmd->argv[0], cmd->argv);
        printf("%s: no such file or directory\n", cmd->argv[0]);
        fflush(stdout);
        exit(1);
    } else if (pid > 0) { // Parent process
        if (cmd->is_bg && bg_allowed) {
            printf("background pid is %d\n", pid);
            fflush(stdout);
            if (bg_count < MAX_BG_PROCS) {
                bg_pids[bg_count++] = pid;
            }
        } else {
            fg_pid = pid;
            int status;
            waitpid(pid, &status, 0);
            fg_pid = 0;
            if (WIFSIGNALED(status)) {
                printf("^Cterminated by signal %d\n", WTERMSIG(status));
                fflush(stdout);
            }
            last_fg_status = status;
        }
    } else {
        perror("fork failed");
    }
}

void reap_background() {
    int status;
    pid_t pid;
    for (int i = 0; i < bg_count; i++) {
        pid = waitpid(bg_pids[i], &status, WNOHANG);
        if (pid > 0) {
            if (WIFEXITED(status)) {
                printf("background pid %d is done: exit value %d\n", pid, WEXITSTATUS(status));
            } else if (WIFSIGNALED(status)) {
                printf("background pid %d is done: terminated by signal %d\n", pid, WTERMSIG(status));
            }
            fflush(stdout);
            for (int j = i; j < bg_count - 1; j++) {
                bg_pids[j] = bg_pids[j + 1];
            }
            bg_count--;
            i--;
        }
    }
}

int main() {
    struct command_line *cmd;

    struct sigaction sa_int = {0};
    struct sigaction sa_tstp = {0};

    sa_int.sa_handler = SIG_IGN;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0;
    if (sigaction(SIGINT, &sa_int, NULL) == -1) {
        perror("sigaction SIGINT failed");
        exit(1);
    }

    sa_tstp.sa_handler = handle_sigtstp;
    sigemptyset(&sa_tstp.sa_mask);
    sa_tstp.sa_flags = 0;
    if (sigaction(SIGTSTP, &sa_tstp, NULL) == -1) {
        perror("sigaction SIGTSTP failed");
        exit(1);
    }

    while (1) {
        reap_background();
        cmd = parse_input();

        if (cmd->argc > 0 && !run_builtin(cmd)) {
            execute_command(cmd);
        }

        free_command(cmd);
    }

    return 0;
}
