/* Guarantee that openpty() works on both MacOS and Linux */
#ifdef __APPLE__
  #include <util.h>
#else
  #include <pty.h>
#endif


#include "pty_session.h"
#include "logger.h"
#include "utils.h"
#include <stdlib.h>
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define BUFFER_SIZE 4096
int create_pseudoterminal(int *master_file_descriptor, int *slave_file_descriptor, char **session_id);
int fork_and_exec_shell(int master_file_descriptor, int slave_file_descriptor);
const char * get_default_shell(void);
const char * get_shell_name(const char *shell_path);
void configure_shell_prompt(const char *shell_name);
ssize_t send_input(char *command, int master_file_descriptor, size_t command_n_bytes);
void read_loop(int master_file_descriptor, void (*on_output)(const char *buffer, ssize_t n_bytes_read, void *context), void *context);
void close_terminal_session(int master_file_descriptor);

/*
 * Create a new pseudoterminal session, this function is just a wrapper of the openpty() function
 * the openpty() function returns the file descriptors of the master and slave pseudoterminals
 * this function just exposes these to the UI Layer 
 * We will use the pseudoterminal_session_id to create a file that will contains the logs of each terminal session
 */
int create_pseudoterminal(int *master_file_descriptor, int *slave_file_descriptor, char **session_id) {
    *session_id = generate_string_uuid_v7();
    if (*session_id == NULL) {
        return -1;
    }

    int openpty_result = openpty(master_file_descriptor, slave_file_descriptor, NULL, NULL, NULL);
    if (openpty_result != 0) {
        free(*session_id);
        *session_id = NULL;
        return openpty_result;
    }

    if (terminal_logger_create(*master_file_descriptor, *session_id) == NULL) {
        close(*master_file_descriptor);
        close(*slave_file_descriptor);
        free(*session_id);
        *session_id = NULL;
        return -1;
    }

    return 0;
}

/*
 * @e need fork and exec in order to make our application alive
 * if we don't fork(), so we create a new child process that is the exact copy of this one,
 * then the exec() will replace the current process, and the app would crash
 */
int fork_and_exec_shell(int master_file_descriptor, int slave_file_descriptor) {
    terminal_logger *logger = terminal_logger_find(master_file_descriptor);
    // pid_t here is just an alias for an integer/long value, depending on the OS
    pid_t child_process_pid = fork();

    if (child_process_pid == -1) { // fork() is failed
        if (logger != NULL) {
            terminal_logger_log(logger, "ERROR", "SHELL_FORK_FAILED", strerror(errno), strlen(strerror(errno)));
        }
        return -1;
    } else if (child_process_pid == 0) {
        // child process, creation of the shell
        // call setsid() to start a new session, of which the child is the session leader
        // this step also causes the child to lose its controlling terminal
        setsid();

        // making slave the controlling terminal
        ioctl(slave_file_descriptor, TIOCSCTTY, 0);

        // use dup() to duplicate the file descriptor for the slave device on
        // STDIN STDOUT and STDERR
        dup2(slave_file_descriptor, STDIN_FILENO);
        dup2(slave_file_descriptor, STDOUT_FILENO);
        dup2(slave_file_descriptor, STDERR_FILENO);
        

        close(master_file_descriptor);
        close(slave_file_descriptor);

        // call exec() to start the terminal oriented program that is to be connected
        // to the pseudoterminal slave
        const char *default_shell = get_default_shell();
        const char *shell_name = get_shell_name(default_shell);

        char login_shell_name[256];
        snprintf(login_shell_name, sizeof(login_shell_name), "-%s", shell_name);
        execlp(default_shell, login_shell_name, NULL);

        _exit(127);
    } else {
        if (logger != NULL) {
            char message[128];
            snprintf(message, sizeof(message), "pid=%d session=%s", child_process_pid, terminal_logger_session_id(logger));
            terminal_logger_log(logger, "INFO", "SHELL_STARTED", message, strlen(message));
        }
        // parent process, it doesn't need the slave
        close(slave_file_descriptor);
        return child_process_pid;
    }
}

const char * get_default_shell(void) {
    const char *shell = getenv("SHELL");

    return shell ? shell : "/bin/sh";
}

const char * get_shell_name(const char *shell_path) {
    const char *shell_name = strrchr(shell_path, '/');

    return shell_name ? shell_name + 1 : shell_path;
}

void configure_shell_prompt(const char *shell_name) {
    setenv("TERM", "xterm-256color", 1);
    setenv("COLORTERM", "truecolor", 1);

    if (strcmp(shell_name, "zsh") == 0) {
        setenv("PS1", "%n@%~ ", 1);
        setenv("PROMPT", "%n@%~ ", 1);
        setenv("PROMPT_EOL_MARK", "", 1);
    } else if (strcmp(shell_name, "bash") == 0) {
        setenv("PS1", "\\u@\\w ", 1);
    } else {
        setenv("PS1", "$USER:$PWD ", 1);
    }
}

/*
 * Send input to the master_fd that sends it to the slave, and the slave shell elaborates
 * and sends to the STDOUT of the slave, that will be read by the master, and then by the user app
 */
ssize_t send_input(char *command, int master_file_descriptor, size_t command_n_bytes) {
    terminal_logger *logger = terminal_logger_find(master_file_descriptor);
    if (logger != NULL) {
        terminal_logger_log(logger, "INFO", "INPUT", command, command_n_bytes);
    }
    return write(master_file_descriptor, command, command_n_bytes);
}

/*
 * Reading the STDOUT connected to the slave connected to the given master
 */
void read_loop(int master_file_descriptor, void (*on_output)(const char *buffer, ssize_t n_bytes_read, void *context), void *context) {
    char buffer[BUFFER_SIZE];
    terminal_logger *logger = terminal_logger_find(master_file_descriptor);

    struct pollfd poll_file_descriptor = { .fd = master_file_descriptor, .events = POLLIN };

    // we pass to poll() the fds to check and the event,
    // the number of the fds to check (in our case 1)
    // and the timeout, that we don't want, so -1 for us
    // if poll returns -1 there is an error, if 0 it means timeout
    // so we check for values > 0, that is the count of the fds that got some operation
    // in our case we just have 1, so we could simplify to == 1, but i don't know if in the future i want
    // to put more fds in this function
    while(poll(&poll_file_descriptor, 1, -1) > 0) {
        // we need to use bitwise AND with POLLIN, becasue there might be also the POLLHUP events (that means connection close) with some output
        // if we use == we will lose this edge case
        if((poll_file_descriptor.revents & POLLIN) > 0) {
            ssize_t n_bytes_read = read(master_file_descriptor, buffer, BUFFER_SIZE);
            
            // some error, as we expect output here given that the bitwise operation is true
            // TODO: add a counter for the error to have some sort of reliability before exiting the loop
            if(n_bytes_read <= 0) break;

            if (logger != NULL) {
                terminal_logger_log(logger, "INFO", "OUTPUT", buffer, (size_t)n_bytes_read);
            }
            on_output(buffer, n_bytes_read, context);
        }
    }

    terminal_logger_close(master_file_descriptor, "SESSION_CLOSED");
}

void close_terminal_session(int master_file_descriptor) {
    close(master_file_descriptor);
    terminal_logger_close(master_file_descriptor, "SESSION_CLOSED");
}
