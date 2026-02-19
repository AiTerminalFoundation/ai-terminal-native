#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <signal.h>
#include <fnctl.h>

static struct winsize windowSize;

/* terminal window resizing operations */
void handleTerminalWindowSizeChangeSignal(int signal);
int getTerminalWindowSize(struct winsize *windowSize);
int setTerminalWindowSize(struct winsize *windowSize);

int createPseudoterminalProcess(int *master_file_descriptor, int *slave_file_descriptor);
int fork_and_exec_shell(void);


int main(void) {
    //register various signals to listen
    signal(SIGWINCH, handleTerminalWindowSizeChangeSignal);

    //getting initial window size
    if (getTerminalWindowSize(&windowSize) == -1) return -1;
:
    printf("Initial size: %d x %d px\n", windowSize.ws_xpixel, windowSize.ws_ypixel);

    while (1) {
        pause();
    }


    return 0;
}

void handleTerminalWindowSizeChangeSignal(int signal) {
    if (getTerminalWindowSize(&windowSize) == -1) {
        puts("Error getting window size");
    } else {
        printf("size: %d x %d px\n", windowSize.ws_xpixel, windowSize.ws_ypixel);
    }
}

int getTerminalWindowSize(struct winsize *windowSize) {
    return ioctl(STDIN_FILENO, TIOCGWINSZ, windowSize);
}

int setTerminalWindowSize(struct winsize *windowSize) {
    puts("Terminal resizing detected");
    return ioctl(STDIN_FILENO, TIOCSWINSZ, windowSize);
}

/* Create a new pseudoterminal session, this function is just a wrapper of the openpty() function
 * the openpty() function returns the file descriptors of the master and slave pseudoterminals
 * this function just exposes these to the UI Layer 
 */
int createPseudoterminalProcess(int *master_file_descriptor, int *slave_file_descriptor) {
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
        pid_t session_id = setsid();

        // use dup() to duplicate the file descriptor for the slave device on
        // STDIN STDOUT and STDERR
        int slave_stdin_file_descriptor = dup(STDIN_FILENO);
        int slave_stdout_file_descriptor = dup(STDOUT_FILENO);
        int slave_stderr_file_descriptor = dup(STDERR_FILENO);
        // call exec() to start the terminal oriented program that is to be connected
        // to the pseudoterminal slave
        return 0;
    } else {
        // parent process, doing cleanup
    }
}
