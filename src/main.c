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

int create_terminal_session(int *master_file_descriptor, int *slave_file_descriptor);
int fork_and_exec_shell(void);


int main(void) {
    //register various signals to listen
    signal(SIGWINCH, handleTerminalWindowSizeChangeSignal);

    //getting initial window size
    if (getTerminalWindowSize(&windowSize) == -1) return -1;
:
    printf("Initial size: %d x %d px\n", windowSize.ws_xpixel, windowSize.ws_ypixel);

    while(1) {
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
int createNewPseudoterminalProcess(int *master_file_descriptor, int *slave_file_descriptor) {
    return openpty(master_file_descriptor, slave_file_descriptor, NULL, NULL, NULL);
}

int fork_and_exec_shell(void) {
    return 0;
}

