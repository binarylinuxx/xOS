#pragma once

#include <stdint.h>
#include <stddef.h>

void keyboard_init();
void keyboard_handler();
char keyboard_get_char();
uint8_t keyboard_has_input();
