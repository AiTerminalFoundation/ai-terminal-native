#include "read_write.h"
#include "logger.h"
#include <stdlib.h>
#include <poll.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include <string.h>


#define BUFFER_SIZE 4096
#define CSI_BUFFER_SIZE 128
#define CSI_INTERMEDIATE_BUFFER_SIZE 8
#define CSI_MAX_PARAMS 16
#define OSC_BUFFER_SIZE 1024
#define TERMINAL_DEFAULT_COLOR -1
#define TERMINAL_TRUECOLOR_FLAG 0x01000000
#define UTF8_REPLACEMENT_CODEPOINT 0xFFFD


typedef enum {
    ACTION_PRINT,
    ACTION_CARRIAGE_RETURN,
    ACTION_LINE_FEED,
    ACTION_BACKSPACE,
    ACTION_SET_GRAPHICS,
    ACTION_CURSOR_UP,
    ACTION_CLEAR_LINE,
    ACTION_SET_MODE,
    ACTION_OSC_TITLE,
    ACTION_IGNORE
} TerminalActionType;

/*
 * Semantic actions produced after a complete CSI sequence is parsed.
 * Parameters and private markers still need to be interpreted when the action
 * is executed. For example, CSI ? 25 h and CSI ? 1049 h both map to SET_MODE.
 */
typedef enum {
    CSI_ACTION_UNKNOWN = 0,

    CSI_ACTION_SGR,                 /* m */
    CSI_ACTION_CURSOR_POSITION,     /* H, f */
    CSI_ACTION_CURSOR_COLUMN,       /* G */
    CSI_ACTION_CURSOR_ROW,          /* d */

    CSI_ACTION_CURSOR_UP,           /* A */
    CSI_ACTION_CURSOR_DOWN,         /* B */
    CSI_ACTION_CURSOR_RIGHT,        /* C */
    CSI_ACTION_CURSOR_LEFT,         /* D */
    CSI_ACTION_CURSOR_NEXT_LINE,    /* E */
    CSI_ACTION_CURSOR_PREVIOUS_LINE,/* F */

    CSI_ACTION_ERASE_DISPLAY,       /* J */
    CSI_ACTION_ERASE_LINE,          /* K */
    CSI_ACTION_ERASE_CHARACTERS,    /* X */

    CSI_ACTION_INSERT_CHARACTERS,   /* @ */
    CSI_ACTION_DELETE_CHARACTERS,   /* P */
    CSI_ACTION_INSERT_LINES,        /* L */
    CSI_ACTION_DELETE_LINES,        /* M */

    CSI_ACTION_SCROLL_UP,           /* S */
    CSI_ACTION_SCROLL_DOWN,         /* T */
    CSI_ACTION_SET_SCROLL_REGION,   /* r */

    CSI_ACTION_SET_MODE,            /* h */
    CSI_ACTION_RESET_MODE,          /* l */
    CSI_ACTION_SAVE_CURSOR,         /* s */
    CSI_ACTION_RESTORE_CURSOR,      /* u */

    CSI_ACTION_DEVICE_STATUS,       /* n */
    CSI_ACTION_DEVICE_ATTRIBUTES,   /* c */
    CSI_ACTION_SET_CURSOR_STYLE     /* SP q */
} CsiAction;

typedef struct {
    CsiAction action;
    int params[CSI_MAX_PARAMS];
    size_t params_count;
    uint8_t private_marker;
} CsiCommand;

typedef struct {
    bool is_bold;
    int32_t color;
} cell_properties;

typedef struct {
    int rows;
    int columns;
    TerminalScreenCell *cells;

    int cursor[2]; // [0] row and [1] col
    int saved_cursor[2];
    int scroll_top;
    int scroll_bottom;
    cell_properties current_properties;
} TerminalScreen;

typedef enum {
    GROUND_STATE,                       /* normal printing output state */
    ESCAPE_STATE,                       /* an escape sequence is started */
    CONTROL_SEQUENCE_INTRODUCER_STATE,  /* the start of a an escape sequence = [ */
    OPERATING_SYSTEM_COMMAND_STATE      /* useful for tab renaming, hyperlink = ] */
} TerminalState;                  /* Actually there are other states to handle but for a minimal working version is enough */

typedef enum {
    ESC_ASCII = 0x1b,
    CONTROL_SEQUENCE_INTRODUCER_ASCII = 0x5b,  /* '[' = hex 5B, decimal 91 */
    OPERATING_SYSTEM_COMMAND_ASCII = 0x5d,     /* ']' = hex 5D, decimal 93 */
    SEMICOLON_ASCII = 0x3b
} ASCII;


struct csi_sequence {
    uint8_t params[CSI_BUFFER_SIZE];
    int params_len;
    uint8_t intermediate_bytes[CSI_INTERMEDIATE_BUFFER_SIZE];
    int intermediate_bytes_len;
    uint8_t final_byte;
};

typedef struct {
    uint32_t codepoint;
    uint32_t min_codepoint;
    int remaining_bytes;
} Utf8Decoder;


static TerminalState terminal_state = GROUND_STATE;
static struct csi_sequence csi_sequence;
static struct csi_sequence *csi_sequence_ptr = &csi_sequence;
static Utf8Decoder utf8_decoder;
static TerminalScreen main_terminal_screen = {.current_properties = {.color = TERMINAL_DEFAULT_COLOR}};
static TerminalScreen alternate_terminal_screen = {.current_properties = {.color = TERMINAL_DEFAULT_COLOR}};
static TerminalScreen *active_terminal_screen = &main_terminal_screen;
static bool osc_escape_pending = false;
static bool application_cursor_keys = false;
#define terminal_screen (*active_terminal_screen)

ssize_t write_bytes(char *bytes, int master_file_descriptor, size_t n_bytes);
int resize_terminal_screen(int rows, int columns);
void read_loop(int master_file_descriptor, void (*on_output)(const TerminalScreenSnapshot *snapshot, void *context), void *context);
void clear_csi_sequence(void);
int parse_csi(uint8_t c);
void parse(int master_fd, uint8_t c);
void clear_buffer(uint8_t *buffer, int *length);
void parse_osc(uint8_t c);
void clear_utf8_decoder(void);
bool is_utf8_continuation_byte(uint8_t byte);
void parse_utf8_byte(uint8_t byte);
char to_utf8(uint8_t bytes[]);
void to_screen_cell(TerminalActionType *terminalActionType);
CsiCommand evaluate_csi_sequence(const struct csi_sequence *sequence);
void execute_csi_command(const CsiCommand *command);
bool parse_csi_parameters(const struct csi_sequence *sequence, CsiCommand *command);
int ascii_digit_to_int(uint8_t byte);
int get_csi_parameter_by_index(const CsiCommand *command, size_t index, int default_value);
int add_clamped_to_int(int value, int amount);
int subtract_clamped_to_zero(int value, int amount);
bool is_sgr_color_component(int value);
int32_t make_sgr_truecolor(int red, int green, int blue);
int resize_single_terminal_screen(TerminalScreen *screen, int rows, int columns);
void terminal_screen_use_alternate(bool enabled);
void terminal_screen_clear(void);
void terminal_screen_clear_cell(TerminalScreenCell *cell);
void terminal_screen_clear_row(int row);
void terminal_screen_clamp_cursor(void);
void terminal_screen_reset_scroll_region(void);
void terminal_screen_set_scroll_region(const CsiCommand *command);
void terminal_screen_new_line(void);
void terminal_screen_scroll_up_one_line(void);
void terminal_screen_scroll_region_up_one_line(int top, int bottom);
void terminal_screen_put_byte(uint8_t byte);
void terminal_screen_put_codepoint(uint32_t codepoint);
void terminal_screen_apply_sgr(const CsiCommand *command);
void terminal_screen_erase_line(int mode);
void terminal_screen_erase_display(int mode);
TerminalScreenSnapshot terminal_screen_snapshot(void);
TerminalScreenSnapshot get_terminal_screen_snapshot(void);


/*
 * Send input to the master_fd that sends it to the slave, and the slave shell elaborates
 * and sends to the STDOUT of the slave, that will be read by the master, and then by the user app
 */
ssize_t write_bytes(char *bytes, int master_file_descriptor, size_t n_bytes) {
    terminal_logger *logger = terminal_logger_find(master_file_descriptor);
    if (logger != NULL) {
        terminal_logger_log(logger, "INFO", "INPUT", bytes, n_bytes);
    }
    return write(master_file_descriptor, bytes, n_bytes);
}

int resize_terminal_screen(int rows, int columns) {
    if (rows <= 0 || columns <= 0) {
        return -1;
    }

    if (resize_single_terminal_screen(&main_terminal_screen, rows, columns) != 0) {
        return -1;
    }

    if (resize_single_terminal_screen(&alternate_terminal_screen, rows, columns) != 0) {
        return -1;
    }

    terminal_screen_clamp_cursor();
    return 0;
}

int resize_single_terminal_screen(TerminalScreen *screen, int rows, int columns) {
    size_t cells_count = (size_t)rows * (size_t)columns;
    if (cells_count > SIZE_MAX / sizeof(TerminalScreenCell)) {
        return -1;
    }

    TerminalScreenCell *new_cells = calloc(cells_count, sizeof(TerminalScreenCell));
    if (new_cells == NULL) {
        return -1;
    }

    for (size_t index = 0; index < cells_count; index++) {
        terminal_screen_clear_cell(&new_cells[index]);
    }

    if (screen->cells != NULL) {
        int rows_to_copy = rows < screen->rows ? rows : screen->rows;
        int columns_to_copy = columns < screen->columns ? columns : screen->columns;

        for (int row = 0; row < rows_to_copy; row++) {
            memcpy(
                &new_cells[row * columns],
                &screen->cells[row * screen->columns],
                (size_t)columns_to_copy * sizeof(TerminalScreenCell)
            );
        }
    }

    free(screen->cells);
    screen->cells = new_cells;
    screen->rows = rows;
    screen->columns = columns;
    screen->scroll_top = 0;
    screen->scroll_bottom = rows - 1;
    if (screen->cursor[0] >= rows) screen->cursor[0] = rows - 1;
    if (screen->cursor[1] >= columns) screen->cursor[1] = columns - 1;
    if (screen->cursor[0] < 0) screen->cursor[0] = 0;
    if (screen->cursor[1] < 0) screen->cursor[1] = 0;
    return 0;
}


/*
 * Reading the STDOUT connected to the slave connected to the given master
 */
// the on_output is going to change, it will basically send a grid, and each entry a[m][n] of the matrix will have what is going to change, so for instance if we have a matrix of
// mxn (let's say 1092x728) and we reduce the window size we will send a new matrix of new_m x new_n with in each of the matrix[i][j] the new chars. so then swiftui should just render a matrix.
// to get the window_size we get it from termios (already implementd in the main one)
// we will need something to parse escape sequences in real characters (it's going to be a big switch?)
// how do we send the cursor position? we are going to send an entire struct to the screen that will be build with (int cursor_i = x, int cursor_j, int cursor_type(i want to be able to render a normal text editor like cursor, char[][] screen) we will send already parsed sequences
void read_loop(int master_file_descriptor, void (*on_output)(const TerminalScreenSnapshot *snapshot, void *context), void *context) {
    char buffer[BUFFER_SIZE];

    terminal_logger *logger = terminal_logger_find(master_file_descriptor);
    clear_csi_sequence();
    clear_utf8_decoder();
    terminal_state = GROUND_STATE;
    application_cursor_keys = false;

    struct pollfd poll_file_descriptor = {
        .fd = master_file_descriptor,
        .events = POLLIN
    };

    // we pass to poll() the fds to check and the event,
    // the number of the fds to check (in our case 1)
    // and the timeout, that we don't want, so -1 for us
    // if poll returns -1 there is an error, if 0 it means timeout
    // so we check for values > 0, that is the count of the fds that got some operation
    // in our caTse we just have 1, so we could simplify to == 1, but i don't know if in the future i want
    // to put more fds in this function
    while(poll(&poll_file_descriptor, 1, -1) > 0) {
        // we need to use bitwise AND with POLLIN, becasue there might be also the POLLHUP events (that means connection close) with some output
        // if we use == we will lose this edge case
        if((poll_file_descriptor.revents & POLLIN) > 0) {
            ssize_t n_bytes_read = read(master_file_descriptor, buffer, BUFFER_SIZE);

            // some error, as we expect output here given that the bitwise operation is true
            // TODO: add a counter for the error to have some sort of reliability before exiting the loop
            if(n_bytes_read <= 0) break;

            if(logger != NULL) {
                terminal_logger_log(logger, "INFO", "OUTPUT", buffer, (size_t)n_bytes_read);
            }

            //parse the data we get inside a state machine
            for(int i = 0; i < n_bytes_read; i++) {
                parse(master_file_descriptor, buffer[i]);
            }

            if (on_output != NULL && terminal_screen.cells != NULL) {
                TerminalScreenSnapshot snapshot = terminal_screen_snapshot();
                on_output(&snapshot, context);
            }
        }
    }

    terminal_logger_close(master_file_descriptor, "SESSION_CLOSED");
}

/* Examples */
/* \x1b[31mRed\x1b[0m World */
/* \r\x1B[0m\x1B[27m\x1B[24m\x1B[Jmicheleverriello@Micheles-MacBook-Pro / % \x1B[K\x1B[?2004h */
void parse(int master_fd, uint8_t c) {
    switch(terminal_state) {
        case GROUND_STATE:
            if(c == ESC_ASCII) {
                clear_utf8_decoder();
                terminal_state = ESCAPE_STATE;
            } else if (c == '\r') {
                clear_utf8_decoder();
                terminal_screen.cursor[1] = 0;
            } else if (c == '\n') {
                clear_utf8_decoder();
                terminal_screen_new_line();
            } else if (c == '\b' || c == 0x7f) {
                clear_utf8_decoder();
                terminal_screen.cursor[1] = subtract_clamped_to_zero(terminal_screen.cursor[1], 1);
            } else if (c == '\t') {
                clear_utf8_decoder();
                int spaces = 8 - (terminal_screen.cursor[1] % 8);
                for (int i = 0; i < spaces; i++) {
                    terminal_screen_put_byte(' ');
                }
            } else if (c >= 0x20) {
                parse_utf8_byte(c);
            } else {
                /* Other C0 controls are ignored for the first rendering pass. */
                clear_utf8_decoder();
            }
            break;
        case ESCAPE_STATE:
            switch (c) {
                case CONTROL_SEQUENCE_INTRODUCER_ASCII: // csi escape sequence starting, clearing the csi_sequence pointer
                    clear_csi_sequence();
                    terminal_state = CONTROL_SEQUENCE_INTRODUCER_STATE;
                    break;
                case OPERATING_SYSTEM_COMMAND_ASCII:
                    osc_escape_pending = false;
                    terminal_state = OPERATING_SYSTEM_COMMAND_STATE;
                    break;
                default:
                    terminal_state = GROUND_STATE;
                    break;
            }
            break;
        case CONTROL_SEQUENCE_INTRODUCER_STATE:
            parse_csi(c);
            break;
        case OPERATING_SYSTEM_COMMAND_STATE:
            parse_osc(c);
            break;
        default:
            // the idea is that in the future we manage other escape sequences other than CSI and OSC
            break;
    }
}

/* will return -1 if the buffer overflows */
/* CSI Sequences are ESC[ 1;2;3 A composed by params [0-9] divided by ; and a final byte in the range */
/*
 * params:        bytes 0x30..0x3F
 * intermediates: bytes 0x20..0x2F
 * final:         byte 0x40..0x7E
 */
int parse_csi(uint8_t c) {
    if (c >= 0x30 && c <= 0x3F) { // param byte
        if (csi_sequence_ptr->params_len >= CSI_BUFFER_SIZE) {
            clear_csi_sequence();
            terminal_state = GROUND_STATE;
            return -1;
        }

        csi_sequence_ptr->params[csi_sequence_ptr->params_len] = c;
        csi_sequence_ptr->params_len++;
    } else if (c >= 0x20 && c <= 0x2F) { // intermediate byte
        if (csi_sequence_ptr->intermediate_bytes_len >= CSI_INTERMEDIATE_BUFFER_SIZE) {
            clear_csi_sequence();
            terminal_state = GROUND_STATE;
            return -1;
        }

        csi_sequence_ptr->intermediate_bytes[csi_sequence_ptr->intermediate_bytes_len] = c;
        csi_sequence_ptr->intermediate_bytes_len++;
    } else if (c >= 0x40 && c <= 0x7E) { // final byte
        csi_sequence_ptr->final_byte = c;
        CsiCommand command = evaluate_csi_sequence(csi_sequence_ptr);
        execute_csi_command(&command);
        terminal_state = GROUND_STATE;
        clear_csi_sequence();
    } else {
        /* Cancel malformed sequences instead of remaining stuck in CSI state. */
        clear_csi_sequence();
        terminal_state = GROUND_STATE;
        return -1;
    }

    return 0;
}


CsiCommand evaluate_csi_sequence(const struct csi_sequence *sequence) {
    CsiCommand command = {
        .action = CSI_ACTION_UNKNOWN,
        .params_count = 0,
        .private_marker = 0
    };

    switch (sequence->final_byte) {
        case '@': command.action = CSI_ACTION_INSERT_CHARACTERS; break;
        case 'A': command.action = CSI_ACTION_CURSOR_UP; break;
        case 'B': command.action = CSI_ACTION_CURSOR_DOWN; break;
        case 'C': command.action = CSI_ACTION_CURSOR_RIGHT; break;
        case 'D': command.action = CSI_ACTION_CURSOR_LEFT; break;
        case 'E': command.action = CSI_ACTION_CURSOR_NEXT_LINE; break;
        case 'F': command.action = CSI_ACTION_CURSOR_PREVIOUS_LINE; break;
        case 'G': command.action = CSI_ACTION_CURSOR_COLUMN; break;
        case 'H':
        case 'f': command.action = CSI_ACTION_CURSOR_POSITION; break;
        case 'J': command.action = CSI_ACTION_ERASE_DISPLAY; break;
        case 'K': command.action = CSI_ACTION_ERASE_LINE; break;
        case 'L': command.action = CSI_ACTION_INSERT_LINES; break;
        case 'M': command.action = CSI_ACTION_DELETE_LINES; break;
        case 'P': command.action = CSI_ACTION_DELETE_CHARACTERS; break;
        case 'S': command.action = CSI_ACTION_SCROLL_UP; break;
        case 'T': command.action = CSI_ACTION_SCROLL_DOWN; break;
        case 'X': command.action = CSI_ACTION_ERASE_CHARACTERS; break;
        case 'c': command.action = CSI_ACTION_DEVICE_ATTRIBUTES; break;
        case 'd': command.action = CSI_ACTION_CURSOR_ROW; break;
        case 'h': command.action = CSI_ACTION_SET_MODE; break;
        case 'l': command.action = CSI_ACTION_RESET_MODE; break;
        case 'm': command.action = CSI_ACTION_SGR; break;
        case 'n': command.action = CSI_ACTION_DEVICE_STATUS; break;
        case 'r': command.action = CSI_ACTION_SET_SCROLL_REGION; break;
        case 's': command.action = CSI_ACTION_SAVE_CURSOR; break;
        case 'u': command.action = CSI_ACTION_RESTORE_CURSOR; break;
        case 'q':
            /* Cursor style is CSI Ps SP q; q alone can mean another command. */
            if (sequence->intermediate_bytes_len == 1 &&
                sequence->intermediate_bytes[0] == 0x20) {
                command.action = CSI_ACTION_SET_CURSOR_STYLE;
            }
            break;
        default:
            break;
    }

    if (!parse_csi_parameters(sequence, &command)) {
        command.action = CSI_ACTION_UNKNOWN;
    }

    return command;
}


bool parse_csi_parameters(const struct csi_sequence *sequence, CsiCommand *command) {
    int value = 0;
    bool has_digit = false;
    int index = 0;

    /*
     * Bytes 0x3C..0x3F are private prefixes. Vim commonly uses '?' for
     * cursor visibility and the alternate screen, for example CSI ? 1049 h.
     */
    if (sequence->params_len > 0 &&
        sequence->params[0] >= 0x3C &&
        sequence->params[0] <= 0x3F) {
        command->private_marker = sequence->params[0];
        index++;
    }

    /* No parameter bytes remain after an optional private marker. */
    if (index == sequence->params_len) {
        return true;
    }

    for (; index < sequence->params_len; index++) {
        uint8_t byte = sequence->params[index];
        int digit = ascii_digit_to_int(byte);

        if (digit >= 0) {
            if (value > (INT_MAX - digit) / 10) return false; // int overflow check

            value = (value * 10) + digit; // adding one less significant digit
            has_digit = true;
        } else if (byte == ';') { // parameter seq terminated, another one starts
            if (command->params_count >= CSI_MAX_PARAMS) return false;

            command->params[command->params_count] = has_digit ? value : 0;
            command->params_count++;
            value = 0;
            has_digit = false;
        } else {
            // colon subparameters are not implemented yet
            return false;
        }
    }

    if (command->params_count >= CSI_MAX_PARAMS) {
        return false;
    }

    command->params[command->params_count] = has_digit ? value : 0;
    command->params_count++;
    return true;
}


/* While working with params (that are ints), we still receive ascii bytes, so we need to convert
 * them into their actual value.
 * Returns -1 if not a digit
 */
int ascii_digit_to_int(uint8_t byte) {
    if (byte < '0' || byte > '9') {
        return -1;
    }

    return byte - '0';
}


/* Returns the index_th parameter in the CSICommand, otherwise a default value */
int get_csi_parameter_by_index(const CsiCommand *command, size_t index, int default_value) {
    if (index >= command->params_count || command->params[index] == 0) {
        return default_value;
    }

    return command->params[index];
}


int add_clamped_to_int(int value, int amount) {
    if (amount > INT_MAX - value) {
        return INT_MAX;
    }

    return value + amount;
}


int subtract_clamped_to_zero(int value, int amount) {
    if (amount >= value) {
        return 0;
    }

    return value - amount;
}

bool is_sgr_color_component(int value) {
    return value >= 0 && value <= 255;
}

int32_t make_sgr_truecolor(int red, int green, int blue) {
    return TERMINAL_TRUECOLOR_FLAG | (red << 16) | (green << 8) | blue;
}


void execute_csi_command(const CsiCommand *command) {
    int amount = get_csi_parameter_by_index(command, 0, 1); // getting the first parameter value and defaulting it to 1 if not available

    switch (command->action) {
        case CSI_ACTION_CURSOR_UP:
            terminal_screen.cursor[0] = subtract_clamped_to_zero(terminal_screen.cursor[0], amount);
            terminal_screen_clamp_cursor();
            break;
        case CSI_ACTION_CURSOR_DOWN:
            terminal_screen.cursor[0] = add_clamped_to_int(terminal_screen.cursor[0], amount);
            terminal_screen_clamp_cursor();
            break;
        case CSI_ACTION_CURSOR_RIGHT:
            terminal_screen.cursor[1] = add_clamped_to_int(terminal_screen.cursor[1], amount);
            terminal_screen_clamp_cursor();
            break;
        case CSI_ACTION_CURSOR_LEFT:
            terminal_screen.cursor[1] = subtract_clamped_to_zero(terminal_screen.cursor[1], amount);
            terminal_screen_clamp_cursor();
            break;
        case CSI_ACTION_CURSOR_NEXT_LINE:
            terminal_screen.cursor[0] = add_clamped_to_int(terminal_screen.cursor[0], amount);
            terminal_screen.cursor[1] = 0;
            terminal_screen_clamp_cursor();
            break;
        case CSI_ACTION_CURSOR_PREVIOUS_LINE:
            terminal_screen.cursor[0] = subtract_clamped_to_zero(terminal_screen.cursor[0], amount);
            terminal_screen.cursor[1] = 0;
            terminal_screen_clamp_cursor();
            break;
        case CSI_ACTION_CURSOR_COLUMN:
            terminal_screen.cursor[1] = amount - 1;
            terminal_screen_clamp_cursor();
            break;
        case CSI_ACTION_CURSOR_ROW:
            terminal_screen.cursor[0] = amount - 1;
            terminal_screen_clamp_cursor();
            break;
        case CSI_ACTION_CURSOR_POSITION:
            /*
             * CSI coordinates are 1-based; the screen grid is 0-based.
             * Missing or zero parameters default to row 1, column 1.
             */
            terminal_screen.cursor[0] = amount - 1;
            terminal_screen.cursor[1] = get_csi_parameter_by_index(command, 1, 1) - 1;
            terminal_screen_clamp_cursor();
            break;
        case CSI_ACTION_ERASE_LINE:
            terminal_screen_erase_line(get_csi_parameter_by_index(command, 0, 0));
            break;
        case CSI_ACTION_ERASE_DISPLAY:
            terminal_screen_erase_display(get_csi_parameter_by_index(command, 0, 0));
            break;
        case CSI_ACTION_SGR:
            terminal_screen_apply_sgr(command);
            break;
        case CSI_ACTION_SET_SCROLL_REGION:
            terminal_screen_set_scroll_region(command);
            break;
        case CSI_ACTION_SET_MODE:
            if (command->private_marker == '?') {
                int mode = get_csi_parameter_by_index(command, 0, 0);
                if (mode == 1) {
                    application_cursor_keys = true;
                } else if (mode == 1049) {
                    terminal_screen_use_alternate(true);
                }
            }
            break;
        case CSI_ACTION_RESET_MODE:
            if (command->private_marker == '?') {
                int mode = get_csi_parameter_by_index(command, 0, 0);
                if (mode == 1) {
                    application_cursor_keys = false;
                } else if (mode == 1049) {
                    terminal_screen_use_alternate(false);
                }
            }
            break;
        case CSI_ACTION_SAVE_CURSOR:
            memcpy(terminal_screen.saved_cursor, terminal_screen.cursor, sizeof(terminal_screen.cursor));
            break;
        case CSI_ACTION_RESTORE_CURSOR:
            memcpy(terminal_screen.cursor, terminal_screen.saved_cursor, sizeof(terminal_screen.cursor));
            terminal_screen_clamp_cursor();
            break;
        default:
            /*
             * Parsing recognizes the remaining actions, but screen mutation,
             * text attributes, terminal modes, and query replies come next.
             */
            break;
    }
}


void clear_csi_sequence(void) {
    clear_buffer(csi_sequence_ptr->params, &csi_sequence_ptr->params_len);
    clear_buffer(csi_sequence_ptr->intermediate_bytes, &csi_sequence_ptr->intermediate_bytes_len);
    csi_sequence_ptr->final_byte = 0;
}


/*
 * Clearing buffer and it's length, once buffer is cleared len is zeroed
 */
void clear_buffer(uint8_t *buffer, int *length) {
    for (int i = 0; i < *length; i++) buffer[i] = 0;
    *length = 0;
}

void clear_utf8_decoder(void) {
    utf8_decoder.codepoint = 0;
    utf8_decoder.min_codepoint = 0;
    utf8_decoder.remaining_bytes = 0;
}

bool is_utf8_continuation_byte(uint8_t byte) {
    return byte >= 0x80 && byte <= 0xBF;
}

void parse_utf8_byte(uint8_t byte) {
    if (utf8_decoder.remaining_bytes == 0) {
        if (byte <= 0x7F) {
            terminal_screen_put_codepoint(byte);
        } else if (byte >= 0xC2 && byte <= 0xDF) {
            utf8_decoder.codepoint = byte & 0x1F;
            utf8_decoder.min_codepoint = 0x80;
            utf8_decoder.remaining_bytes = 1;
        } else if (byte >= 0xE0 && byte <= 0xEF) {
            utf8_decoder.codepoint = byte & 0x0F;
            utf8_decoder.min_codepoint = 0x800;
            utf8_decoder.remaining_bytes = 2;
        } else if (byte >= 0xF0 && byte <= 0xF4) {
            utf8_decoder.codepoint = byte & 0x07;
            utf8_decoder.min_codepoint = 0x10000;
            utf8_decoder.remaining_bytes = 3;
        } else {
            terminal_screen_put_codepoint(UTF8_REPLACEMENT_CODEPOINT);
        }
        return;
    }

    if (!is_utf8_continuation_byte(byte)) {
        terminal_screen_put_codepoint(UTF8_REPLACEMENT_CODEPOINT);
        clear_utf8_decoder();
        parse_utf8_byte(byte);
        return;
    }

    utf8_decoder.codepoint = (utf8_decoder.codepoint << 6) | (byte & 0x3F);
    utf8_decoder.remaining_bytes--;

    if (utf8_decoder.remaining_bytes == 0) {
        uint32_t codepoint = utf8_decoder.codepoint;
        uint32_t min_codepoint = utf8_decoder.min_codepoint;
        clear_utf8_decoder();

        if (codepoint < min_codepoint ||
            codepoint > 0x10FFFF ||
            (codepoint >= 0xD800 && codepoint <= 0xDFFF)) {
            terminal_screen_put_codepoint(UTF8_REPLACEMENT_CODEPOINT);
            return;
        }

        terminal_screen_put_codepoint(codepoint);
    }
}

void terminal_screen_use_alternate(bool enabled) {
    if (enabled) {
        active_terminal_screen = &alternate_terminal_screen;
        terminal_screen_reset_scroll_region();
        terminal_screen_clear();
    } else {
        active_terminal_screen = &main_terminal_screen;
    }

    terminal_screen_clamp_cursor();
}

void terminal_screen_clear(void) {
    if (terminal_screen.cells == NULL) {
        return;
    }

    size_t cells_count = (size_t)terminal_screen.rows * (size_t)terminal_screen.columns;
    for (size_t index = 0; index < cells_count; index++) {
        terminal_screen_clear_cell(&terminal_screen.cells[index]);
    }

    terminal_screen.cursor[0] = 0;
    terminal_screen.cursor[1] = 0;
}

void terminal_screen_clear_cell(TerminalScreenCell *cell) {
    cell->codepoint = ' ';
    cell->is_empty = 1;
    cell->is_bold = 0;
    cell->color = TERMINAL_DEFAULT_COLOR;
}

void terminal_screen_clear_row(int row) {
    if (terminal_screen.cells == NULL || row < 0 || row >= terminal_screen.rows) {
        return;
    }

    for (int column = 0; column < terminal_screen.columns; column++) {
        terminal_screen_clear_cell(&terminal_screen.cells[row * terminal_screen.columns + column]);
    }
}

void terminal_screen_clamp_cursor(void) {
    if (terminal_screen.rows <= 0 || terminal_screen.columns <= 0) {
        terminal_screen.cursor[0] = 0;
        terminal_screen.cursor[1] = 0;
        return;
    }

    if (terminal_screen.cursor[0] < 0) terminal_screen.cursor[0] = 0;
    if (terminal_screen.cursor[1] < 0) terminal_screen.cursor[1] = 0;
    if (terminal_screen.cursor[0] >= terminal_screen.rows) terminal_screen.cursor[0] = terminal_screen.rows - 1;
    if (terminal_screen.cursor[1] >= terminal_screen.columns) terminal_screen.cursor[1] = terminal_screen.columns - 1;
}

void terminal_screen_reset_scroll_region(void) {
    terminal_screen.scroll_top = 0;
    terminal_screen.scroll_bottom = terminal_screen.rows > 0 ? terminal_screen.rows - 1 : 0;
}

void terminal_screen_set_scroll_region(const CsiCommand *command) {
    if (terminal_screen.rows <= 0) {
        return;
    }

    int top = get_csi_parameter_by_index(command, 0, 1) - 1;
    int bottom = get_csi_parameter_by_index(command, 1, terminal_screen.rows) - 1;

    if (top < 0) top = 0;
    if (bottom >= terminal_screen.rows) bottom = terminal_screen.rows - 1;

    if (top >= bottom) {
        return;
    }

    terminal_screen.scroll_top = top;
    terminal_screen.scroll_bottom = bottom;
    terminal_screen.cursor[0] = 0;
    terminal_screen.cursor[1] = 0;
}

void terminal_screen_new_line(void) {
    if (terminal_screen.cells == NULL) {
        return;
    }

    if (terminal_screen.cursor[0] == terminal_screen.scroll_bottom) {
        terminal_screen_scroll_up_one_line();
    } else if (terminal_screen.cursor[0] < terminal_screen.rows - 1) {
        terminal_screen.cursor[0]++;
    } else {
        terminal_screen_scroll_region_up_one_line(0, terminal_screen.rows - 1);
    }
}

void terminal_screen_scroll_up_one_line(void) {
    terminal_screen_scroll_region_up_one_line(terminal_screen.scroll_top, terminal_screen.scroll_bottom);
}

void terminal_screen_scroll_region_up_one_line(int top, int bottom) {
    if (terminal_screen.cells == NULL || terminal_screen.rows <= 0 || terminal_screen.columns <= 0) {
        return;
    }

    if (top < 0) top = 0;
    if (bottom >= terminal_screen.rows) bottom = terminal_screen.rows - 1;
    if (top >= bottom) {
        return;
    }

    if (bottom > top) {
        memmove(
            &terminal_screen.cells[top * terminal_screen.columns],
            &terminal_screen.cells[(top + 1) * terminal_screen.columns],
            (size_t)(bottom - top) * (size_t)terminal_screen.columns * sizeof(TerminalScreenCell)
        );
    }

    terminal_screen_clear_row(bottom);
}

void terminal_screen_put_byte(uint8_t byte) {
    terminal_screen_put_codepoint(byte);
}

void terminal_screen_put_codepoint(uint32_t codepoint) {
    if (terminal_screen.cells == NULL || terminal_screen.rows <= 0 || terminal_screen.columns <= 0) {
        return;
    }

    terminal_screen_clamp_cursor();

    int row = terminal_screen.cursor[0];
    int column = terminal_screen.cursor[1];
    TerminalScreenCell *cell = &terminal_screen.cells[row * terminal_screen.columns + column];
    cell->codepoint = codepoint;
    cell->is_empty = 0;
    cell->is_bold = terminal_screen.current_properties.is_bold ? 1 : 0;
    cell->color = terminal_screen.current_properties.color;

    terminal_screen.cursor[1]++;
    if (terminal_screen.cursor[1] >= terminal_screen.columns) {
        terminal_screen.cursor[1] = 0;
        terminal_screen_new_line();
    }
}

void terminal_screen_apply_sgr(const CsiCommand *command) {
    if (command->params_count == 0) {
        terminal_screen.current_properties.is_bold = false;
        terminal_screen.current_properties.color = TERMINAL_DEFAULT_COLOR;
        return;
    }

    for (size_t index = 0; index < command->params_count; index++) {
        int parameter = command->params[index];

        if (parameter == 0) {
            terminal_screen.current_properties.is_bold = false;
            terminal_screen.current_properties.color = TERMINAL_DEFAULT_COLOR;
        } else if (parameter == 1) {
            terminal_screen.current_properties.is_bold = true;
        } else if (parameter == 22) {
            terminal_screen.current_properties.is_bold = false;
        } else if (parameter >= 30 && parameter <= 37) {
            terminal_screen.current_properties.color = parameter - 30;
        } else if (parameter >= 90 && parameter <= 97) {
            terminal_screen.current_properties.color = parameter - 90 + 8;
        } else if (parameter == 39) {
            terminal_screen.current_properties.color = TERMINAL_DEFAULT_COLOR;
        } else if (parameter == 38 && index + 2 < command->params_count &&
                   command->params[index + 1] == 5) {
            int color = command->params[index + 2];
            if (is_sgr_color_component(color)) {
                terminal_screen.current_properties.color = color;
            }
            index += 2;
        } else if (parameter == 38 && index + 4 < command->params_count &&
                   command->params[index + 1] == 2) {
            int red = command->params[index + 2];
            int green = command->params[index + 3];
            int blue = command->params[index + 4];
            if (is_sgr_color_component(red) &&
                is_sgr_color_component(green) &&
                is_sgr_color_component(blue)) {
                terminal_screen.current_properties.color = make_sgr_truecolor(red, green, blue);
            }
            index += 4;
        }
    }
}

void terminal_screen_erase_line(int mode) {
    if (terminal_screen.cells == NULL) {
        return;
    }

    terminal_screen_clamp_cursor();

    int row = terminal_screen.cursor[0];
    int start_column = 0;
    int end_column = terminal_screen.columns - 1;

    if (mode == 0) {
        start_column = terminal_screen.cursor[1];
    } else if (mode == 1) {
        end_column = terminal_screen.cursor[1];
    } else if (mode != 2) {
        return;
    }

    for (int column = start_column; column <= end_column; column++) {
        terminal_screen_clear_cell(&terminal_screen.cells[row * terminal_screen.columns + column]);
    }
}

void terminal_screen_erase_display(int mode) {
    if (terminal_screen.cells == NULL) {
        return;
    }

    terminal_screen_clamp_cursor();

    if (mode == 2 || mode == 3) {
        terminal_screen_clear();
        return;
    }

    int cursor_row = terminal_screen.cursor[0];
    int cursor_column = terminal_screen.cursor[1];

    if (mode == 0) {
        for (int row = cursor_row; row < terminal_screen.rows; row++) {
            int start_column = row == cursor_row ? cursor_column : 0;
            for (int column = start_column; column < terminal_screen.columns; column++) {
                terminal_screen_clear_cell(&terminal_screen.cells[row * terminal_screen.columns + column]);
            }
        }
    } else if (mode == 1) {
        for (int row = 0; row <= cursor_row; row++) {
            int end_column = row == cursor_row ? cursor_column : terminal_screen.columns - 1;
            for (int column = 0; column <= end_column; column++) {
                terminal_screen_clear_cell(&terminal_screen.cells[row * terminal_screen.columns + column]);
            }
        }
    }
}

TerminalScreenSnapshot terminal_screen_snapshot(void) {
    TerminalScreenSnapshot snapshot = {
        .rows = terminal_screen.rows,
        .columns = terminal_screen.columns,
        .cursor_row = terminal_screen.cursor[0],
        .cursor_column = terminal_screen.cursor[1],
        .application_cursor_keys = application_cursor_keys ? 1 : 0,
        .cells = terminal_screen.cells
    };

    return snapshot;
}

TerminalScreenSnapshot get_terminal_screen_snapshot(void) {
    return terminal_screen_snapshot();
}


void parse_osc(uint8_t c) {
    if (osc_escape_pending) {
        if (c == '\\') {
            terminal_state = GROUND_STATE;
        }

        osc_escape_pending = false;
        return;
    }

    if (c == 0x07) {
        terminal_state = GROUND_STATE;
        return;
    }

    if (c == ESC_ASCII) {
        osc_escape_pending = true;
    }
}
