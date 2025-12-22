#include "print.h"
#include "shell.h"
#include "keyboard.h"
#include "bfs.h"

void kernel_main() {
	print_str("Kernel started\n");
	keyboard_init();
	//in order to-do
	//bfs_init();  // Initialize the filesystem
	//print_str("BFS initialized\n");
	print_clear();
	print_str("Print clear done\n");
	print_str("About to run shell\n");
	shell_run();
	print_str("Welcome to bOS\n");
}