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
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define BUFFER_SIZE 4096
int create_pseudoterminal(int *master_file_descriptor, int *slave_file_descriptor, char **session_id);
int fork_and_exec_shell(int master_file_descriptor, int slave_file_descriptor);
const char * get_default_shell(void);
const char * get_shell_name(const char *shell_path);
void configure_and_exec_shell(void);
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
e*/
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

        configure_and_exec_shell();
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

void configure_and_exec_shell(void) {
    const char *default_shell = get_default_shell();
    const char *shell_name = get_shell_name(default_shell);
    char login_shell_name[256];

    setenv("TERM", "xterm-256color", 1);
    setenv("COLORTERM", "truecolor", 1);

    snprintf(login_shell_name, sizeof(login_shell_name), "-%s", shell_name);
    execlp(default_shell, login_shell_name, NULL);
}

void close_terminal_session(int master_file_descriptor) {
    close(master_file_descriptor);
    terminal_logger_close(master_file_descriptor, "SESSION_CLOSED");
}
