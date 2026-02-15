#include <sys/ioctl.h>
#include <unistd.h>

int getTerminalWindowSize(struct winsize *windowSize) {
    if(ioctl(STDIN_FILENO, TIOCGWINSZ, windowSize) == -1) return -1;
    return 0; 
}

int setTerminalWindowSize(struct winsize *windowSize) {
    if(ioctl(STDIN_FILENO, TIOCSWINSZ, windowSize) == -1) return -1;
    return 0;
}

