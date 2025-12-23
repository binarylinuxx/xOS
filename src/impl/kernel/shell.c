#include "shell.h"
#include "print.h"
#include "keyboard.h"
#include "bfs.h"

#define MAX_COMMAND_LENGTH 256
#define MAX_USERNAME_LENGTH 32
#define MAX_HOSTNAME_LENGTH 32
#define CURSOR_BLINK_RATE 30000

static char command_buffer[MAX_COMMAND_LENGTH];
static size_t command_pos = 0;

static char username[MAX_USERNAME_LENGTH] = "user";
static char hostname[MAX_HOSTNAME_LENGTH] = "machine";

static size_t cursor_row = 0;
static size_t cursor_col = 0;
static uint8_t cursor_visible = 0;
static uint32_t cursor_blink_counter = 0;

// String helper functions
static size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len]) len++;
    return len;
}

static int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

static int strncmp(const char* s1, const char* s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

static void strcpy(char* dest, const char* src) {
    while ((*dest++ = *src++));
}

static void strncpy(char* dest, const char* src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0';
    }
}

// Path helper function
static void strip_trailing_slash(char* path) {
    size_t len = strlen(path);
    if (len > 0 && path[len - 1] == '/') {
        path[len - 1] = '\0';
    }
}

// Number helper functions
static int is_digit(char c) {
    return c >= '0' && c <= '9';
}

static int atoi(const char* str) {
    int result = 0;
    int sign = 1;

    // Skip whitespace
    while (*str == ' ' || *str == '\t') str++;

    // Handle sign
    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }

    // Convert digits
    while (is_digit(*str)) {
        result = result * 10 + (*str - '0');
        str++;
    }

    return sign * result;
}

static void itoa(int num, char* str) {
    int i = 0;
    int is_negative = 0;

    if (num == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return;
    }

    if (num < 0) {
        is_negative = 1;
        num = -num;
    }

    while (num != 0) {
        str[i++] = (num % 10) + '0';
        num = num / 10;
    }

    if (is_negative) {
        str[i++] = '-';
    }

    str[i] = '\0';

    // Reverse the string
    int start = 0;
    int end = i - 1;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }
}

static void draw_cursor(uint8_t show) {
    // Save current position
    size_t saved_row, saved_col;
    print_get_cursor(&saved_row, &saved_col);

    // Draw cursor at cursor position
    print_set_cursor(cursor_row, cursor_col);
    print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
    print_char(show ? '_' : ' ');

    // Restore position (cursor drawing moved it forward by 1)
    print_set_cursor(cursor_row, cursor_col);
}

static void update_cursor_position() {
    print_get_cursor(&cursor_row, &cursor_col);
}

static void print_prompt() {
    print_set_color(PRINT_COLOR_LIGHT_CYAN, PRINT_COLOR_BLACK);
    print_str("[");
    print_set_color(PRINT_COLOR_LIGHT_GREEN, PRINT_COLOR_BLACK);
    print_str(username);
    print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
    print_str(">");
    print_set_color(PRINT_COLOR_LIGHT_MAGENTA, PRINT_COLOR_BLACK);
    print_str(hostname);
    print_set_color(PRINT_COLOR_LIGHT_CYAN, PRINT_COLOR_BLACK);
    print_str("]");
    print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
    print_str("> ");
    update_cursor_position();
}

static void execute_help() {
    print_set_color(PRINT_COLOR_YELLOW, PRINT_COLOR_BLACK);
    print_str("Available commands:\n");
    print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
    print_str("  help               - Show this help message\n");
    print_str("  clear              - Clear the screen\n");
    print_str("  echo <text>        - Print text\n");
    print_str("  info               - Show system information\n");
    print_str("  calc <expr>        - Calculator (e.g., calc 5 + 3)\n");
    print_str("  setusername <name> - Set username\n");
    print_str("  sethost <name>     - Set hostname\n");
    print_str("  ls [dir]           - List files and directories\n");
    print_str("  cat <file>         - Display file contents\n");
    print_str("  write <file> <text>- Create/write file\n");
    print_str("  mkdir <dir>        - Create directory\n");
}

static void execute_clear() {
    print_clear();
}

static void execute_echo(const char* args) {
    if (*args) {
        print_str((char*)args);
        print_str("\n");
    }
}

static void execute_info() {
    print_set_color(PRINT_COLOR_LIGHT_CYAN, PRINT_COLOR_BLACK);
    print_str("xOS - x Operating System\n");
    print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
    print_str("Version: 0.1.0\n");
    print_str("Architecture: x86_64\n");
    print_str("Mode: Long Mode (64-bit)\n");
}

static void execute_calc(const char* args) {
    if (!*args) {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        print_str("Usage: calc <num1> <op> <num2>\n");
        print_str("Operators: + - * / %\n");
        print_str("Example: calc 10 + 5\n");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return;
    }

    // Parse first number
    const char* p = args;
    while (*p == ' ') p++;

    if (!is_digit(*p) && *p != '-') {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        print_str("Error: Invalid first number\n");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return;
    }

    int num1 = atoi(p);

    // Skip to operator
    if (*p == '-' && is_digit(*(p + 1))) p++;
    while (is_digit(*p)) p++;
    while (*p == ' ') p++;

    // Get operator
    char op = *p;
    if (op != '+' && op != '-' && op != '*' && op != '/' && op != '%') {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        print_str("Error: Invalid operator (use + - * / %)\n");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return;
    }
    p++;

    // Parse second number
    while (*p == ' ') p++;

    if (!is_digit(*p) && *p != '-') {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        print_str("Error: Invalid second number\n");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return;
    }

    int num2 = atoi(p);

    // Calculate result
    int result;
    int valid = 1;

    switch (op) {
        case '+':
            result = num1 + num2;
            break;
        case '-':
            result = num1 - num2;
            break;
        case '*':
            result = num1 * num2;
            break;
        case '/':
            if (num2 == 0) {
                print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
                print_str("Error: Division by zero\n");
                print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
                valid = 0;
            } else {
                result = num1 / num2;
            }
            break;
        case '%':
            if (num2 == 0) {
                print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
                print_str("Error: Modulo by zero\n");
                print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
                valid = 0;
            } else {
                result = num1 % num2;
            }
            break;
        default:
            valid = 0;
    }

    if (valid) {
        char result_str[32];
        itoa(result, result_str);

        print_set_color(PRINT_COLOR_LIGHT_GREEN, PRINT_COLOR_BLACK);
        print_str("Result: ");
        print_str(result_str);
        print_str("\n");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
    }
}

static void execute_setusername(const char* args) {
    if (*args) {
        strncpy(username, args, MAX_USERNAME_LENGTH - 1);
        username[MAX_USERNAME_LENGTH - 1] = '\0';
        print_set_color(PRINT_COLOR_GREEN, PRINT_COLOR_BLACK);
        print_str("Username set to: ");
        print_str(username);
        print_str("\n");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
    } else {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        print_str("Error: Username cannot be empty\n");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
    }
}

static void execute_sethost(const char* args) {
    if (*args) {
        strncpy(hostname, args, MAX_HOSTNAME_LENGTH - 1);
        hostname[MAX_HOSTNAME_LENGTH - 1] = '\0';
        print_set_color(PRINT_COLOR_GREEN, PRINT_COLOR_BLACK);
        print_str("Hostname set to: ");
        print_str(hostname);
        print_str("\n");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
    } else {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        print_str("Error: Hostname cannot be empty\n");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
    }
}

static void execute_ls(const char* args) {
    char buffer[1024];
    const char* path = (*args) ? args : "/";

    int len = bfs_list_files(path, buffer, sizeof(buffer));
    if (len > 0) {
        // Parse and color the output
        char* ptr = buffer;
        while (*ptr) {
            char* line_start = ptr;
            // Find end of line
            while (*ptr && *ptr != '\n') ptr++;

            // Check if this is a directory (ends with /)
            int is_dir = 0;
            if (ptr > line_start && *(ptr - 1) == '/') {
                is_dir = 1;
            }

            // Print with appropriate color
            if (is_dir) {
                print_set_color(PRINT_COLOR_LIGHT_BLUE, PRINT_COLOR_BLACK);
            } else {
                print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
            }

            // Print the line
            char saved = *ptr;
            *ptr = '\0';
            print_str(line_start);
            *ptr = saved;

            if (*ptr == '\n') {
                print_char('\n');
                ptr++;
            }
        }
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
    } else {
        print_set_color(PRINT_COLOR_YELLOW, PRINT_COLOR_BLACK);
        print_str("No files found.\n");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
    }
}

static void execute_cat(const char* args) {
    if (*args) {
        // Make a copy to strip trailing slash
        char path[256];
        strncpy(path, args, sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';
        strip_trailing_slash(path);

        char buffer[1024];
        int len = bfs_read_file(path, buffer, sizeof(buffer));
        if (len >= 0) {
            print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
            print_str(buffer);
            print_str("\n");
        } else {
            print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
            print_str("Error: File not found or is a directory\n");
            print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        }
    } else {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        print_str("Usage: cat <filename>\n");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
    }
}

static void execute_write(const char* args) {
    if (*args) {
        // Copy args to a mutable buffer
        char args_copy[256];
        strncpy(args_copy, args, sizeof(args_copy) - 1);
        args_copy[sizeof(args_copy) - 1] = '\0';

        // Parse filename and content
        char* filename = args_copy;
        char* content = args_copy;
        while (*content && *content != ' ') content++;

        if (*content == ' ') {
            *content = '\0'; // Null-terminate filename
            content++;
            while (*content == ' ') content++;

            if (*content) {
                // Strip trailing slash from filename
                strip_trailing_slash(filename);

                int result = bfs_create_file(filename, content);
                if (result == 0) {
                    print_set_color(PRINT_COLOR_GREEN, PRINT_COLOR_BLACK);
                    print_str("File created: ");
                    print_str(filename);
                    print_str("\n");
                    print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
                } else {
                    print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
                    print_str("Error: Could not create file\n");
                    print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
                }
            } else {
                print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
                print_str("Error: Content cannot be empty\n");
                print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
            }
        } else {
            print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
            print_str("Usage: write <filename> <content>\n");
            print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        }
    } else {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        print_str("Usage: write <filename> <content>\n");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
    }
}

static void execute_mkdir(const char* args) {
    if (*args) {
        // Make a copy to strip trailing slash
        char path[256];
        strncpy(path, args, sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';
        strip_trailing_slash(path);

        int result = bfs_create_directory(path);
        if (result == 0) {
            print_set_color(PRINT_COLOR_GREEN, PRINT_COLOR_BLACK);
            print_str("Directory created: ");
            print_str(path);
            print_str("/\n");
            print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        } else {
            print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
            print_str("Error: Could not create directory\n");
            print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        }
    } else {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        print_str("Usage: mkdir <dirname>\n");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
    }
}

static void execute_command() {
    command_buffer[command_pos] = '\0';

    // Trim leading spaces
    char* cmd = command_buffer;
    while (*cmd == ' ' || *cmd == '\t') cmd++;

    if (*cmd == '\0') {
        return;
    }

    // Find the command and arguments
    char* args = cmd;
    while (*args && *args != ' ' && *args != '\t') args++;
    if (*args) {
        *args = '\0';
        args++;
        while (*args == ' ' || *args == '\t') args++;
    }

    if (strcmp(cmd, "help") == 0) {
        execute_help();
    } else if (strcmp(cmd, "clear") == 0) {
        execute_clear();
    } else if (strcmp(cmd, "echo") == 0) {
        execute_echo(args);
    } else if (strcmp(cmd, "info") == 0) {
        execute_info();
    } else if (strcmp(cmd, "calc") == 0) {
        execute_calc(args);
    } else if (strcmp(cmd, "setusername") == 0) {
        execute_setusername(args);
    } else if (strcmp(cmd, "sethost") == 0) {
        execute_sethost(args);
    } else if (strcmp(cmd, "ls") == 0) {
        execute_ls(args);
    } else if (strcmp(cmd, "cat") == 0) {
        execute_cat(args);
    } else if (strcmp(cmd, "write") == 0) {
        execute_write(args);
    } else if (strcmp(cmd, "mkdir") == 0) {
        execute_mkdir(args);
    } else {
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_RED);
        print_str("Unknown command: ");
        print_str(cmd);
        print_str("\n");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
    }
}

void shell_run() {
    print_init();
    command_pos = 0;
    print_prompt();
    cursor_visible = 1;
    cursor_blink_counter = 0;

    while (1) {
        // Handle cursor blinking
        cursor_blink_counter++;
        if (cursor_blink_counter >= CURSOR_BLINK_RATE) {
            draw_cursor(0); // Erase old cursor
            cursor_visible = !cursor_visible;
            cursor_blink_counter = 0;
        }
        draw_cursor(cursor_visible);

        char c = keyboard_get_char();

        if (c != 0) {
            draw_cursor(0); // Erase cursor before updating

            if (c == '\n') {
                print_char('\n');
                execute_command();
                command_pos = 0;
                print_prompt();
                cursor_visible = 1;
                cursor_blink_counter = 0;
            } else if (c == '\b') {
                if (command_pos > 0) {
                    command_pos--;
                    print_char('\b');
                    update_cursor_position();
                }
            } else if (c >= 32 && c <= 126 && command_pos < MAX_COMMAND_LENGTH - 1) {
                command_buffer[command_pos++] = c;
                print_char(c);
                update_cursor_position();
            }
        }

        // Small delay to prevent spinning too fast
        for (volatile int i = 0; i < 1000; i++);
    }
}
