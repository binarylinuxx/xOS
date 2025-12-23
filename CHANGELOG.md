# changelog

all notable changes to xOS will be documented here

## [unreleased]

### added
- hierarchical filesystem (BFS) with full directory support
- path parsing for nested directories like hi/subdir/file.txt
- parent directory tracking in inodes
- ls command now accepts directory argument (ls, ls /, ls hi)
- cat command supports full paths
- write command creates files in proper directories
- mkdir command creates directories with path support
- colored output for ls command (directories in blue, files in white)
- proper handling of trailing slashes in paths
- string utility functions (strlen, strcmp, strcpy, strncpy, strncmp) in libc

### changed
- rewrote filesystem from zig to pure C for better stability
- reduced RAM disk size from 512KB to 64KB to prevent boot issues
- filesystem operations now use paths instead of flat names
- bfs_list_files now only lists contents of specified directory
- root directory created automatically on filesystem init

### fixed
- boot loop issue caused by zig global pointer initialization
- filesystem now properly isolates directories instead of flat listing
- memory issues with large static arrays in kernel
- trailing slash handling in file and directory operations

## [0.1.0] - initial release

### added
- x86_64 long mode kernel
- VGA text mode driver (80x25 color text)
- PS/2 keyboard driver (polling based)
- interactive shell with command parsing
- basic commands: help, clear, echo, info, calc, setusername, sethost
- multiboot2 bootloader support
- GRUB2 integration
- calculator with basic operations (+, -, *, /, %)
- colored terminal output
- customizable username and hostname in shell prompt
- blinking cursor in shell
