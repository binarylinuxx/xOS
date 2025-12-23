#include "bfs.h"
#include "string.h"

#define BFS_SUPERBLOCK_BLOCK 0
#define BFS_INODES_START_BLOCK 1
#define BFS_INODES_BLOCKS ((BFS_MAX_INODES * sizeof(bfs_inode_t) + BFS_BLOCK_SIZE - 1) / BFS_BLOCK_SIZE)
#define BFS_DATA_START_BLOCK (BFS_INODES_START_BLOCK + BFS_INODES_BLOCKS)
#define ROOT_INODE 0

static uint8_t ram_disk[BFS_TOTAL_BLOCKS * BFS_BLOCK_SIZE];
static bfs_superblock_t* superblock;
static bfs_inode_t* inodes;
static uint8_t inode_used[BFS_MAX_INODES];
static uint32_t next_free_block;

// Helper: Split path into components
// Returns number of components, modifies path_copy
static int split_path(const char* path, char components[][BFS_MAX_FILENAME], int max_components) {
    if (!path || !*path) return 0;

    int count = 0;
    char path_copy[256];
    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';

    char* token = path_copy;
    char* next_slash;

    while (*token && count < max_components) {
        // Skip leading slashes
        while (*token == '/') token++;
        if (!*token) break;

        // Find next slash
        next_slash = token;
        while (*next_slash && *next_slash != '/') next_slash++;

        // Copy component
        int len = next_slash - token;
        if (len >= BFS_MAX_FILENAME) len = BFS_MAX_FILENAME - 1;
        memcpy(components[count], token, len);
        components[count][len] = '\0';
        count++;

        token = next_slash;
    }

    return count;
}

// Helper: Find inode by name in specific parent directory
static int find_inode_in_dir(const char* name, int parent_idx) {
    for (int i = 0; i < BFS_MAX_INODES; i++) {
        if (inode_used[i] &&
            inodes[i].parent_inode == parent_idx &&
            strcmp(inodes[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

// Helper: Navigate path and return final inode index
// If create_parent is true, creates missing parent directories
static int navigate_path(const char* path, int* parent_idx, char* filename, int create_parent) {
    char components[16][BFS_MAX_FILENAME];
    int num_components = split_path(path, components, 16);

    if (num_components == 0) {
        *parent_idx = -1;
        filename[0] = '\0';
        return ROOT_INODE;
    }

    int current = ROOT_INODE;

    // Navigate through all but last component
    for (int i = 0; i < num_components - 1; i++) {
        int next = find_inode_in_dir(components[i], current);

        if (next < 0) {
            if (create_parent) {
                // Create missing directory
                next = -1;
                for (int j = 0; j < BFS_MAX_INODES; j++) {
                    if (!inode_used[j]) {
                        next = j;
                        break;
                    }
                }
                if (next < 0) return -1; // No free inodes

                strncpy(inodes[next].name, components[i], BFS_MAX_FILENAME - 1);
                inodes[next].name[BFS_MAX_FILENAME - 1] = '\0';
                inodes[next].parent_inode = current;
                inodes[next].is_directory = 1;
                inodes[next].size = 0;
                inode_used[next] = 1;
                superblock->inode_count++;
            } else {
                return -1; // Directory not found
            }
        }

        if (!inodes[next].is_directory) {
            return -1; // Not a directory
        }

        current = next;
    }

    *parent_idx = current;
    strncpy(filename, components[num_components - 1], BFS_MAX_FILENAME - 1);
    filename[BFS_MAX_FILENAME - 1] = '\0';

    return current;
}

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

    // Create root directory
    strncpy(inodes[ROOT_INODE].name, "/", BFS_MAX_FILENAME);
    inodes[ROOT_INODE].parent_inode = -1;
    inodes[ROOT_INODE].is_directory = 1;
    inodes[ROOT_INODE].size = 0;
    inode_used[ROOT_INODE] = 1;
    superblock->inode_count++;
}

int bfs_create_file(const char* path, const char* content) {
    int parent_idx;
    char filename[BFS_MAX_FILENAME];

    // Navigate to parent directory
    if (navigate_path(path, &parent_idx, filename, 0) < 0) {
        return -1; // Parent directory not found
    }

    // Check if file already exists
    if (find_inode_in_dir(filename, parent_idx) >= 0) {
        return -1; // File exists
    }

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
    if (blocks_needed == 0) blocks_needed = 1; // At least one block
    if (next_free_block + blocks_needed > BFS_TOTAL_BLOCKS) return -1; // Not enough space

    bfs_inode_t* inode = &inodes[inode_idx];
    strncpy(inode->name, filename, BFS_MAX_FILENAME - 1);
    inode->name[BFS_MAX_FILENAME - 1] = '\0';
    inode->size = content_len;
    inode->is_directory = 0;
    inode->parent_inode = parent_idx;

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

int bfs_read_file(const char* path, char* buffer, size_t size) {
    int parent_idx;
    char filename[BFS_MAX_FILENAME];

    // Navigate to file
    if (navigate_path(path, &parent_idx, filename, 0) < 0) {
        return -1; // Path not found
    }

    // Find the file
    int inode_idx = find_inode_in_dir(filename, parent_idx);
    if (inode_idx < 0 || inodes[inode_idx].is_directory) {
        return -1; // File not found or is directory
    }

    bfs_inode_t* inode = &inodes[inode_idx];
    size_t to_read = (inode->size < size) ? inode->size : size;
    uint8_t* data = ram_disk + inode->blocks[0] * BFS_BLOCK_SIZE;
    memcpy(buffer, data, to_read);
    buffer[to_read] = '\0'; // Null-terminate for safety
    return to_read;
}

int bfs_list_files(const char* path, char* buffer, size_t size) {
    // Determine which directory to list
    int dir_idx = ROOT_INODE;

    if (path && *path && strcmp(path, "/") != 0) {
        int parent_idx;
        char dirname[BFS_MAX_FILENAME];

        // Navigate to the directory
        if (navigate_path(path, &parent_idx, dirname, 0) < 0) {
            return -1; // Path not found
        }

        // Find the directory inode
        dir_idx = find_inode_in_dir(dirname, parent_idx);
        if (dir_idx < 0 || !inodes[dir_idx].is_directory) {
            return -1; // Not a directory
        }
    }

    // List all entries in this directory
    char* ptr = buffer;
    size_t remaining = size;

    for (int i = 0; i < BFS_MAX_INODES; i++) {
        if (inode_used[i] && inodes[i].parent_inode == dir_idx) {
            size_t name_len = strlen(inodes[i].name);
            size_t entry_len = name_len + 1; // name + newline
            if (inodes[i].is_directory) {
                entry_len++; // Add 1 for '/' suffix
            }
            if (remaining > entry_len) {
                strcpy(ptr, inodes[i].name);
                ptr += name_len;
                // Add '/' for directories
                if (inodes[i].is_directory) {
                    *ptr++ = '/';
                }
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

int bfs_create_directory(const char* path) {
    int parent_idx;
    char dirname[BFS_MAX_FILENAME];

    // Navigate to parent directory
    if (navigate_path(path, &parent_idx, dirname, 0) < 0) {
        return -1; // Parent directory not found
    }

    // Check if directory already exists
    if (find_inode_in_dir(dirname, parent_idx) >= 0) {
        return -1; // Directory exists
    }

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
    strncpy(inode->name, dirname, BFS_MAX_FILENAME - 1);
    inode->name[BFS_MAX_FILENAME - 1] = '\0';
    inode->size = 0;
    inode->is_directory = 1;
    inode->parent_inode = parent_idx;
    // No blocks allocated for directories

    inode_used[inode_idx] = 1;
    superblock->inode_count++;

    return 0;
}