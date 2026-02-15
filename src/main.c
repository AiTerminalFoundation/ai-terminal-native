#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "core/terminalui.h"

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

