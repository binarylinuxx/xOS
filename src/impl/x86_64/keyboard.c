#include "keyboard.h"

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64
#define KEYBOARD_BUFFER_SIZE 256

// Scancodes for shift keys
#define LEFT_SHIFT_SCANCODE 0x2A
#define RIGHT_SHIFT_SCANCODE 0x36

// Normal (non-shifted) characters
static const char scancode_to_ascii[] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' '
};

// Shifted characters
static const char scancode_to_ascii_shift[] = {
    0, 0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' '
};

static uint8_t key_state[256] = {0};
static uint8_t shift_pressed = 0;
static char keyboard_buffer[KEYBOARD_BUFFER_SIZE];
static int buffer_head = 0;
static int buffer_tail = 0;
static int buffer_count = 0;

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

void keyboard_init() {
    // Clear any pending keyboard data
    while (inb(KEYBOARD_STATUS_PORT) & 0x01) {
        inb(KEYBOARD_DATA_PORT);
    }

    // Enable keyboard interface
    outb(KEYBOARD_STATUS_PORT, 0xAE);

    // Enable keyboard scanning
    while (inb(KEYBOARD_STATUS_PORT) & 0x02); // Wait for input buffer empty
    outb(KEYBOARD_DATA_PORT, 0xF4);

    // Clear key states
    for (int i = 0; i < 256; i++) {
        key_state[i] = 0;
    }
    shift_pressed = 0;
}

void keyboard_handler() {
    // Not used in polling mode
}

char keyboard_get_char() {
    char last_char = 0;

    while (1) {
        // Check if data is available
        uint8_t status = inb(KEYBOARD_STATUS_PORT);
        if ((status & 0x01) == 0) {
            break; // No more data
        }

        uint8_t scancode = inb(KEYBOARD_DATA_PORT);

        // Handle key release
        if (scancode & 0x80) {
            uint8_t key = scancode & 0x7F;
            key_state[key] = 0;

            // Handle shift release
            if (key == LEFT_SHIFT_SCANCODE || key == RIGHT_SHIFT_SCANCODE) {
                shift_pressed = 0;
            }
        } else {
            // Handle key press
            if (scancode == LEFT_SHIFT_SCANCODE || scancode == RIGHT_SHIFT_SCANCODE) {
                shift_pressed = 1;
                key_state[scancode] = 1;
            } else if (scancode < sizeof(scancode_to_ascii)) {
                // Return shifted or normal character based on shift state
                last_char = shift_pressed ? scancode_to_ascii_shift[scancode] : scancode_to_ascii[scancode];
            }
        }
    }

    return last_char;
}

uint8_t keyboard_has_input() {
    uint8_t status = inb(KEYBOARD_STATUS_PORT);
    return (status & 0x01) != 0;
}
