/* Guarantee that openpty() works on both MacOS and Linux */
#ifdef __APPLE__
  #include <util.h>
#else
  #include <pty.h>
#endif


#include "terminal.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/uio.h>

#define BUFFER_SIZE 4096


int create_pseudoterminal(int *master_file_descriptor, int *slave_file_descriptor);
int fork_and_exec_shell(int master_file_descriptor, int slave_file_descriptor);
const char * get_default_shell(void);
ssize_t send_input(char *command, int master_file_descriptor, size_t command_n_bytes);
void read_loop(int master_file_descriptor, void (*on_output)(const char *buffer, ssize_t n_bytes_read, void *context), void *context);

/*
 * Create a new pseudoterminal session, this function is just a wrapper of the openpty() function
 * the openpty() function returns the file descriptors of the master and slave pseudoterminals
 * this function just exposes these to the UI Layer 
 */
int create_pseudoterminal(int *master_file_descriptor, int *slave_file_descriptor) {
    return openpty(master_file_descriptor, slave_file_descriptor, NULL, NULL, NULL);
}

/*
 * @e need fork and exec in order to make our application alive
 * if we don't fork(), so we create a new child process that is the exact copy of this one,
 * then the exec() will replace the current process, and the app would crash
 */
int fork_and_exec_shell(int master_file_descriptor, int slave_file_descriptor) {
    // pid_t here is just an alias for an integer/long value, depending on the OS
    pid_t child_process_pid = fork();

    if (child_process_pid == -1) { // fork() is failed
        return -1;
    }
    else if (child_process_pid == 0) {
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
        
        setenv("PS1", "__PROMPT__:$(pwd)> ", 1);       

        execlp(default_shell, default_shell, NULL);

        return 0;
    } else {
        // parent process, it doesn't need the slave
        close(slave_file_descriptor);
        return child_process_pid;
    }
}

const char * get_default_shell(void) {
    const char *shell = getenv("SHELL");

    return shell ? shell : "/bin/sh";
}

/*
 * Send input to the master_fd that sends it to the slave, and the slave shell elaborates
 * and sends to the STDOUT of the slave, that will be read by the master, and then by the user app
 */
ssize_t send_input(char *command, int master_file_descriptor, size_t command_n_bytes) {
    return write(master_file_descriptor, command, command_n_bytes);
}

/*
 * Reading the STDOUT connected to the slave connected to the given master
 */
void read_loop(int master_file_descriptor, void (*on_output)(const char *buffer, ssize_t n_bytes_read, void *context), void *context) {
    char buffer[BUFFER_SIZE];

    struct pollfd poll_file_descriptor = { .fd = master_file_descriptor, .events = POLLIN };

    // we pass to poll() the fds to check and the event,
    // the number of the fds to check (in our case 1)
    // and the timeout, that we don't want, so -1 for us
    // if poll returns -1 there is an error, if 0 it means timeout
    // so we check for values > 0, that is the count of the fds that got some operation
    // in our case we just have 1, so we could simplify to == 1, but i don't know if in the future i want
    // to put more fds in this function
    while(poll(&poll_file_descriptor, 1, -1) > 0) {
        // we need to use bitwise AND with POLLIN, becasue the might be also the POLLHUP events (that means connection close) with some output
        // if we use == we will lose this edge case
        if((poll_file_descriptor.revents & POLLIN) > 0) {
            ssize_t n_bytes_read = read(master_file_descriptor, buffer, BUFFER_SIZE);
            
            // some error, as we expect output here given that the bitwise operation is true
            // TODO: add a counter for the error to have some sort of reliability before exiting the loop
            if(n_bytes_read <= 0) break;

            on_output(buffer, n_bytes_read, context);
        }
    }
}

