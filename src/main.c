#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <signal.h>

static struct winsize windowSize;


void handleTerminalWindowSizeChangeSignal(int signal);
int getTerminalWindowSize(struct winsize *windowSize);
int setTerminalWindowSize(struct winsize *windowSize);


int main(void) {
    //register various signals to listen
    signal(SIGWINCH, handleTerminalWindowSizeChangeSignal);

    //getting initial window size
    if (getTerminalWindowSize(&windowSize) == -1) return -1;

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


