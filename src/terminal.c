
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


int create_pseudoterminal(int *master_file_descriptor, int *slave_file_descriptor);
int fork_and_exec_shell(int master_file_descriptor, int slave_file_descriptor);
char * get_default_shell(void);


/* Create a new pseudoterminal session, this function is just a wrapper of the openpty() function
 * the openpty() function returns the file descriptors of the master and slave pseudoterminals
 * this function just exposes these to the UI Layer 
 */
int create_pseudoterminal(int *master_file_descriptor, int *slave_file_descriptor) {
    return openpty(master_file_descriptor, slave_file_descriptor, NULL, NULL, NULL);
}

/* we need fork and exec in order to make our application alive
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
        puts("Terminal resizing detected");
        char *default_shell = get_default_shell();
        execlp(default_shell, default_shell, NULL);

        return 0;
    } else {
        // parent process, it doesn't need the slave
        close(slave_file_descriptor);
        return child_process_pid;
    }
}

char * get_default_shell(void) {
    char *shell = getenv("SHELL");
    
    if (shell == NULL) {
        shell = "/bin/sh";
    }

    return shell;
}
