#include "print.h"
#include "shell.h"
#include "keyboard.h"
#include "bfs.h"

void kernel_main() {
	print_str("Kernel started\n");
	keyboard_init();
	bfs_init();
	print_clear();
	shell_run();
	print_str("Welcome to bOS\n");
}