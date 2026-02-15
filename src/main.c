#include <stdio.h>
#include "core/core.h"
#include <termios.h>
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

int main(void) {
    struct winsize windowSize;

    if(getTerminalWindowSize(&windowSize) == -1) return -1; 

    printf("Initial window size\n");
    printf("rows: %d, cols: %d, horizontal pixels: %dpx, vertical pixels: %dpx\n\n", windowSize.ws_row, windowSize.ws_col, windowSize.ws_xpixel, windowSize.ws_ypixel);

    for(int i = 0; i < 15; i++) {
        windowSize.ws_col -= 5;
        if(setTerminalWindowSize(&windowSize) == -1) return -1;
        printf("rows: %d, cols: %d, horizontal pixels: %dpx, vertical pixels: %dpx\n\n", windowSize.ws_row, windowSize.ws_col, windowSize.ws_xpixel, windowSize.ws_ypixel);
    }

}

