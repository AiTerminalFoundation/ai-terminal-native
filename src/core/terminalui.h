#ifndef TERMINALUI_H
#define TERMINALUI_H

#include <sys/ioctl.h>
#include <unistd.h>

int getTerminalWindowSize(struct winsize *windowSize);
int setTerminalWindowSize(struct winsize *windowSize);

#endif
