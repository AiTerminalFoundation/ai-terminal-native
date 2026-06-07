#include "read_write.h"
#include "logger.h"
#include <stdlib.h>
#include <poll.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>

#define BUFFER_SIZE 4096
#define CSI_BUFFER_SIZE 128
#define CSI_INTERMEDIATE_BUFFER_SIZE 8
#define CSI_MAX_PARAMS 16
#define OSC_BUFFER_SIZE 1024


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
    int row;
    int column;
} grid_position;

typedef struct {
    CsiAction action;
    int params[CSI_MAX_PARAMS];
    size_t params_count;
    uint8_t private_marker;
} CsiCommand;

typedef struct {
    bool is_bold;
} cell_properties;

typedef struct {
    bool is_empty_cell;
    char character;
    grid_position position;
    cell_properties properties;
} screen_cell;

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


static TerminalState terminal_state = GROUND_STATE;
static cell_properties current_properties;
static grid_position cursor_position = {.row = 0, .column = 0};
static grid_position saved_cursor_position = {.row = 0, .column = 0};
static struct csi_sequence csi_sequence;
static struct csi_sequence *csi_sequence_ptr = &csi_sequence;


ssize_t write_bytes(char *bytes, int master_file_descriptor, size_t n_bytes);
void read_loop(int master_file_descriptor, void (*on_output)(const char *buffer, ssize_t n_bytes_read, void *context), void *context);
void clear_csi_sequence(void);
int parse_csi(uint8_t c);
void parse(int master_fd, uint8_t c);
void clear_buffer(uint8_t *buffer, int *length);
void parse_osc(uint8_t c);
char to_utf8(uint8_t bytes[]);
void to_screen_cell(TerminalActionType *terminalActionType);
CsiCommand evaluate_csi_sequence(const struct csi_sequence *sequence);
void execute_csi_command(const CsiCommand *command);
bool parse_csi_parameters(const struct csi_sequence *sequence, CsiCommand *command);
int ascii_digit_to_int(uint8_t byte);
int csi_parameter(const CsiCommand *command, size_t index, int default_value);
int add_clamped_to_int(int value, int amount);
int subtract_clamped_to_zero(int value, int amount);


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


/*
 * Reading the STDOUT connected to the slave connected to the given master
 */
// the on_output is going to change, it will basically send a grid, and each entry a[m][n] of the matrix will have what is going to change, so for instance if we have a matrix of 
// mxn (let's say 1092x728) and we reduce the window size we will send a new matrix of new_m x new_n with in each of the matrix[i][j] the new chars. so then swiftui should just render a matrix.
// to get the window_size we get it from termios (already implementd in the main one)
// we will need something to parse escape sequences in real characters (it's going to be a big switch?)
// how do we send the cursor position? we are going to send an entire struct to the screen that will be build with (int cursor_i = x, int cursor_j, int cursor_type(i want to be able to render a normal text editor like cursor, char[][] screen) we will send already parsed sequences
void read_loop(int master_file_descriptor, void (*on_output)(const char *buffer, ssize_t n_bytes_read, void *context), void *context) {
    char buffer[BUFFER_SIZE];

    terminal_logger *logger = terminal_logger_find(master_file_descriptor);
    clear_csi_sequence();
    terminal_state = GROUND_STATE;
    cursor_position = (grid_position){.row = 0, .column = 0};
    saved_cursor_position = cursor_position;

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
                terminal_state = ESCAPE_STATE;
            } else {
                TerminalActionType terminal_action_type = ACTION_PRINT;
                (void)terminal_action_type;
                // what else here?
            }
            break;
        case ESCAPE_STATE:
            switch (c) {
                case CONTROL_SEQUENCE_INTRODUCER_ASCII: // csi escape sequence starting, clearing the csi_sequence pointer
                    clear_csi_sequence();
                    terminal_state = CONTROL_SEQUENCE_INTRODUCER_STATE;
                    break;
                case OPERATING_SYSTEM_COMMAND_ASCII:
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


void execute_csi_command(const CsiCommand *command) {
    int amount = get_csi_parameter_by_index(command, 0, 1); // getting the first parameter defaulting it to 1 if not available

    switch (command->action) {
        case CSI_ACTION_CURSOR_UP:
            cursor_position.row = subtract_clamped_to_zero(cursor_position.row, amount);
            break;
        case CSI_ACTION_CURSOR_DOWN:
            cursor_position.row = add_clamped_to_int(cursor_position.row, amount);
            break;
        case CSI_ACTION_CURSOR_RIGHT:
            cursor_position.column = add_clamped_to_int(cursor_position.column, amount);
            break;
        case CSI_ACTION_CURSOR_LEFT:
            cursor_position.column = subtract_clamped_to_zero(cursor_position.column, amount);
            break;
        case CSI_ACTION_CURSOR_NEXT_LINE:
            cursor_position.row = add_clamped_to_int(cursor_position.row, amount);
            cursor_position.column = 0;
            break;
        case CSI_ACTION_CURSOR_PREVIOUS_LINE:
            cursor_position.row = subtract_clamped_to_zero(cursor_position.row, amount);
            cursor_position.column = 0;
            break;
        case CSI_ACTION_CURSOR_COLUMN:
            cursor_position.column = amount - 1;
            break;
        case CSI_ACTION_CURSOR_ROW
            cursor_position.row = amount - 1;
            break;
        case CSI_ACTION_CURSOR_POSITION:
            /*
             * CSI coordinates are 1-based; the screen grid is 0-based.
             * Missing or zero parameters default to row 1, column 1.
             */
            cursor_position.row = amount - 1;
            cursor_position.column = get_csi_parameter_by_index(command, 1, 1) - 1;
            break;
        case CSI_ACTION_SAVE_CURSOR:
            saved_cursor_position = cursor_position;
            break;
        case CSI_ACTION_RESTORE_CURSOR:
            cursor_position = saved_cursor_position;
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


//TODO
void parse_osc(uint8_t c) {
}


//TODO
char to_utf8(uint8_t bytes[]) {
    return 0;
}


//TODO
void to_screen_cell(TerminalActionType *terminalActionType) {
}
