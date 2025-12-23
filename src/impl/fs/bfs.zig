const c = @cImport({
    @cInclude("bfs.h");
});

const std = @import("std");

const RAM_DISK_SIZE = c.BFS_TOTAL_BLOCKS * c.BFS_BLOCK_SIZE;
var ram_disk: [RAM_DISK_SIZE]u8 = undefined;

var superblock: *c.bfs_superblock_t = undefined;
var inodes: [*c]c.bfs_inode_t = undefined;
var data_blocks: [*]u8 = undefined;

// Helper functions
fn find_free_inode() i32 {
    for (0..c.BFS_MAX_INODES) |i| {
        if (inodes[i].size == 0 and inodes[i].name[0] == 0) {
            return @intCast(i);
        }
    }
    return -1;
}

fn find_free_block() i32 {
    const free_blocks: u32 = superblock.free_blocks;
    if (free_blocks > 0) {
        // Corrected algorithm: allocate from the end of data area backward
        // next_block = end_of_data_area - remaining_free_blocks + 1
        // Data area is from data_start_block to (BFS_TOTAL_BLOCKS-1)
        // So last data block is at BFS_TOTAL_BLOCKS-1
        const last_data_block: u32 = c.BFS_TOTAL_BLOCKS - 1;
        const next_block: u32 = last_data_block - (free_blocks - 1);  // -1 to adjust indexing
        
        superblock.free_blocks -= 1;
        return @intCast(next_block);
    }
    return -1;
}

fn find_inode_by_name(name: [*c]const u8) i32 {
    const name_slice = std.mem.sliceTo(name, 0);
    for (0..c.BFS_MAX_INODES) |i| {
        const inode_name_slice = std.mem.sliceTo(&inodes[i].name, 0);
        if (std.mem.eql(u8, name_slice, inode_name_slice)) {
            return @intCast(i);
        }
    }
    return -1;
}

// Public API
export fn bfs_init() void {
    // Initialize pointers to ram_disk sections
    superblock = @alignCast(@ptrCast(&ram_disk));
    inodes = @alignCast(@ptrCast(&ram_disk[c.BFS_BLOCK_SIZE]));
    const data_start = c.BFS_BLOCK_SIZE * (1 + ((@sizeOf(c.bfs_inode_t) * c.BFS_MAX_INODES + c.BFS_BLOCK_SIZE - 1) / c.BFS_BLOCK_SIZE));
    data_blocks = @ptrCast(&ram_disk[data_start]);

    @memset(&ram_disk, 0);

    superblock.* = .{ 
        .magic = c.BFS_MAGIC,
        .total_blocks = c.BFS_TOTAL_BLOCKS,
        .free_blocks = c.BFS_TOTAL_BLOCKS - 1 - ((@sizeOf(c.bfs_inode_t) * c.BFS_MAX_INODES + c.BFS_BLOCK_SIZE - 1) / c.BFS_BLOCK_SIZE),
        .inode_count = 0,
        .data_start_block = 1 + ((@sizeOf(c.bfs_inode_t) * c.BFS_MAX_INODES + c.BFS_BLOCK_SIZE - 1) / c.BFS_BLOCK_SIZE),
    };

    // Create root directory
    const root_inode = find_free_inode();
    if (root_inode >= 0) {
        const root_name = "/";
        const dest_slice = inodes[@intCast(root_inode)].name[0..root_name.len];
        @memcpy(dest_slice, root_name);
        inodes[@intCast(root_inode)].name[root_name.len] = 0;
        inodes[@intCast(root_inode)].is_directory = 1;
        inodes[@intCast(root_inode)].size = 0;
        superblock.inode_count += 1;
    }
}

export fn bfs_create_file(name: [*c]const u8, content: [*c]const u8) c_int {
    if (find_inode_by_name(name) >= 0) {
        return -1; // File exists
    }

    const content_len = std.mem.sliceTo(content, 0).len;
    if (content_len == 0) {
        return -1; // Empty content
    }

    const inode_idx = find_free_inode();
    if (inode_idx < 0) {
        return -1; // No free inodes
    }

    const blocks_needed: u32 = std.math.divTrunc(u32, @intCast(content_len + c.BFS_BLOCK_SIZE - 1), c.BFS_BLOCK_SIZE) catch unreachable;
    if (blocks_needed > c.BFS_INODE_DIRECT_BLOCKS) {
        return -1; // Too large for direct blocks
    }

    // Allocate blocks
    for (0..blocks_needed) |i| {
        const block = find_free_block();
        if (block < 0) {
            return -1; // No free blocks
        }
        inodes[@intCast(inode_idx)].blocks[i] = @intCast(block);
    }

    // Write content
    const data = data_blocks;
    const dest = data + (inodes[@intCast(inode_idx)].blocks[0] - superblock.data_start_block) * c.BFS_BLOCK_SIZE;
    const dest_slice = dest[0..content_len];
    const src_slice = content[0..content_len];
    @memcpy(dest_slice, src_slice);

    const name_slice = std.mem.sliceTo(name, 0);
    const dest_slice_name = inodes[@intCast(inode_idx)].name[0..name_slice.len];
    @memcpy(dest_slice_name, name_slice);
    inodes[@intCast(inode_idx)].name[name_slice.len] = 0;
    inodes[@intCast(inode_idx)].size = @intCast(content_len);
    inodes[@intCast(inode_idx)].is_directory = 0;
    superblock.inode_count += 1;

    return 0;
}

export fn bfs_read_file(name: [*c]const u8, buffer: [*c]u8, size: usize) c_int {
    const inode_idx = find_inode_by_name(name);
    if (inode_idx < 0 or inodes[@intCast(inode_idx)].is_directory != 0) {
        return -1; // Not found or is directory
    }

    const to_read = if (inodes[@intCast(inode_idx)].size < size) inodes[@intCast(inode_idx)].size else @as(u32, @intCast(size));
    const data = data_blocks + (inodes[@intCast(inode_idx)].blocks[0] - superblock.data_start_block) * c.BFS_BLOCK_SIZE;
    const dest_slice = buffer[0..to_read];
    const src_slice = data[0..to_read];
    @memcpy(dest_slice, src_slice);
    buffer[to_read] = 0;

    return @intCast(to_read);
}

export fn bfs_list_files(buffer: [*c]u8, size: usize) c_int {
    var pos: usize = 0;
    for (0..c.BFS_MAX_INODES) |i| {
        if (inodes[i].name[0] != 0) {
            const name_slice = std.mem.sliceTo(&inodes[i].name, 0);
            if (pos + name_slice.len + 2 >= size) {
                break;
            }
            const dest_slice = buffer[pos..pos + name_slice.len];
            @memcpy(dest_slice, name_slice);
            pos += name_slice.len;
            if (inodes[i].is_directory != 0) {
                buffer[pos] = '/';
                pos += 1;
            }
            buffer[pos] = '\n';
            pos += 1;
        }
    }
    buffer[pos] = 0;
    return @intCast(pos);
}

export fn bfs_create_directory(name: [*c]const u8) c_int {
    if (find_inode_by_name(name) >= 0) {
        return -1; // Already exists
    }

    const inode_idx = find_free_inode();
    if (inode_idx < 0) {
        return -1; // No free inodes
    }

    const name_slice = std.mem.sliceTo(name, 0);
    const dest_slice = inodes[@intCast(inode_idx)].name[0..name_slice.len];
    @memcpy(dest_slice, name_slice);
    inodes[@intCast(inode_idx)].name[name_slice.len] = 0;
    inodes[@intCast(inode_idx)].size = 0;
    inodes[@intCast(inode_idx)].is_directory = 1;
    superblock.inode_count += 1;

    return 0;
}
