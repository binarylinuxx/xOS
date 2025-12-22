#include "bfs.h"

#define BFS_SUPERBLOCK_BLOCK 0
#define BFS_INODES_START_BLOCK 1
#define BFS_INODES_BLOCKS ((BFS_MAX_INODES * sizeof(bfs_inode_t) + BFS_BLOCK_SIZE - 1) / BFS_BLOCK_SIZE)
#define BFS_DATA_START_BLOCK (BFS_INODES_START_BLOCK + BFS_INODES_BLOCKS)

static uint8_t ram_disk[BFS_TOTAL_BLOCKS * BFS_BLOCK_SIZE];
static bfs_superblock_t* superblock;
static bfs_inode_t* inodes;
static uint8_t inode_used[BFS_MAX_INODES];
static uint32_t next_free_block;

void bfs_init() {
    memset(ram_disk, 0, sizeof(ram_disk));
    memset(inode_used, 0, sizeof(inode_used));
    superblock = (bfs_superblock_t*)(ram_disk + BFS_SUPERBLOCK_BLOCK * BFS_BLOCK_SIZE);
    inodes = (bfs_inode_t*)(ram_disk + BFS_INODES_START_BLOCK * BFS_BLOCK_SIZE);
    superblock->magic = BFS_MAGIC;
    superblock->total_blocks = BFS_TOTAL_BLOCKS;
    superblock->free_blocks = BFS_TOTAL_BLOCKS - BFS_DATA_START_BLOCK;
    superblock->inode_count = 0;
    superblock->data_start_block = BFS_DATA_START_BLOCK;
    next_free_block = BFS_DATA_START_BLOCK;
}

int bfs_create_file(const char* name, const char* content) {
    // Find free inode
    int inode_idx = -1;
    for (int i = 0; i < BFS_MAX_INODES; i++) {
        if (!inode_used[i]) {
            inode_idx = i;
            break;
        }
    }
    if (inode_idx == -1) return -1; // No free inode

    size_t content_len = strlen(content);
    if (content_len > BFS_INODE_DIRECT_BLOCKS * BFS_BLOCK_SIZE) return -1; // File too large

    // Calculate blocks needed
    uint32_t blocks_needed = (content_len + BFS_BLOCK_SIZE - 1) / BFS_BLOCK_SIZE;
    if (next_free_block + blocks_needed > BFS_TOTAL_BLOCKS) return -1; // Not enough space

    bfs_inode_t* inode = &inodes[inode_idx];
    strncpy(inode->name, name, BFS_MAX_FILENAME - 1);
    inode->name[BFS_MAX_FILENAME - 1] = '\0';
    inode->size = content_len;
    inode->is_directory = 0;

    // Allocate blocks
    for (uint32_t i = 0; i < blocks_needed; i++) {
        inode->blocks[i] = next_free_block++;
    }

    // Write content
    uint8_t* data = ram_disk + inode->blocks[0] * BFS_BLOCK_SIZE;
    memcpy(data, content, content_len);

    inode_used[inode_idx] = 1;
    superblock->inode_count++;
    superblock->free_blocks -= blocks_needed;

    return 0;
}

int bfs_read_file(const char* name, char* buffer, size_t size) {
    // Find inode
    int inode_idx = -1;
    for (int i = 0; i < BFS_MAX_INODES; i++) {
        if (inode_used[i] && strcmp(inodes[i].name, name) == 0 && !inodes[i].is_directory) {
            inode_idx = i;
            break;
        }
    }
    if (inode_idx == -1) return -1; // File not found or is directory

    bfs_inode_t* inode = &inodes[inode_idx];
    size_t to_read = (inode->size < size) ? inode->size : size;
    uint8_t* data = ram_disk + inode->blocks[0] * BFS_BLOCK_SIZE;
    memcpy(buffer, data, to_read);
    buffer[to_read] = '\0'; // Null-terminate for safety
    return to_read;
}

int bfs_list_files(char* buffer, size_t size) {
    char* ptr = buffer;
    size_t remaining = size;
    for (int i = 0; i < BFS_MAX_INODES; i++) {
        if (inode_used[i]) {
            size_t name_len = strlen(inodes[i].name);
            size_t entry_len = name_len + 1; // name + newline
            if (remaining > entry_len) {
                strcpy(ptr, inodes[i].name);
                ptr += name_len;
                *ptr++ = '\n';
                remaining -= entry_len;
            } else {
                break;
            }
        }
    }
    *ptr = '\0';
    return ptr - buffer;
}

int bfs_create_directory(const char* name) {
    // Find free inode
    int inode_idx = -1;
    for (int i = 0; i < BFS_MAX_INODES; i++) {
        if (!inode_used[i]) {
            inode_idx = i;
            break;
        }
    }
    if (inode_idx == -1) return -1; // No free inode

    bfs_inode_t* inode = &inodes[inode_idx];
    strncpy(inode->name, name, BFS_MAX_FILENAME - 1);
    inode->name[BFS_MAX_FILENAME - 1] = '\0';
    inode->size = 0;
    inode->is_directory = 1;
    // No blocks allocated for directories

    inode_used[inode_idx] = 1;
    superblock->inode_count++;

    return 0;
}