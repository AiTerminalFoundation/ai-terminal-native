#ifndef read_write_h
#define read_write_h

#include <stdint.h>
#include <unistd.h>

typedef struct {
    uint32_t codepoint;
    uint8_t is_empty;
    uint8_t is_bold;
    int32_t color;
} TerminalScreenCell;

typedef struct {
    int rows;
    int columns;
    int cursor_row;
    int cursor_column;
    int application_cursor_keys;
    const TerminalScreenCell *cells;
} TerminalScreenSnapshot;

ssize_t write_bytes(char *bytes, int master_file_descriptor, size_t n_bytes);
int resize_terminal_screen(int rows, int columns);
TerminalScreenSnapshot get_terminal_screen_snapshot(void);
void read_loop(int master_file_descriptor, void (*on_output)(const TerminalScreenSnapshot *snapshot, void *context), void *context);

#endif
