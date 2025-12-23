#pragma once

#include <stdint.h>
#include <stddef.h>

#define BFS_MAGIC 0x42465300  // "BFS\0"
#define BFS_BLOCK_SIZE 512
#define BFS_MAX_FILENAME 32
#define BFS_INODE_DIRECT_BLOCKS 10
#define BFS_MAX_INODES 64
#define BFS_TOTAL_BLOCKS 128  // 64KB RAM disk (reduced from 512KB)

// Superblock structure
typedef struct {
    uint32_t magic;
    uint32_t total_blocks;
    uint32_t free_blocks;
    uint32_t inode_count;
    uint32_t data_start_block;
} bfs_superblock_t;

// Inode structure
typedef struct {
    uint32_t size;
    uint32_t blocks[BFS_INODE_DIRECT_BLOCKS];
    char name[BFS_MAX_FILENAME];
    uint8_t is_directory;
    int32_t parent_inode;  // -1 for root, otherwise index of parent directory
} bfs_inode_t;

// File system functions
void bfs_init();
int bfs_create_file(const char* path, const char* content);
int bfs_read_file(const char* path, char* buffer, size_t size);
int bfs_list_files(const char* path, char* buffer, size_t size);
int bfs_create_directory(const char* path);