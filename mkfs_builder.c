// Build: gcc -O2 -std=c17 -Wall -Wextra mkfs_builder.c minivsfs_utils.c -o mkfs_builder
#include "minivsfs.h"

int parse_cli_args(int argc, char* argv[], cli_args_builder_t* args) {
    // Initialize defaults
    args->image_name = NULL;
    args->size_kib = 0;
    args->inode_count = 0;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--image") == 0) {
            if (i + 1 >= argc) {
                print_error("--image requires a filename");
                return -1;
            }
            args->image_name = argv[++i];
        }
        else if (strcmp(argv[i], "--size-kib") == 0) {
            if (i + 1 >= argc) {
                print_error("--size-kib requires a value");
                return -1;
            }
            args->size_kib = (uint32_t)atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "--inodes") == 0) {
            if (i + 1 >= argc) {
                print_error("--inodes requires a value");
                return -1;
            }
            args->inode_count = (uint32_t)atoi(argv[++i]);
        }
        else {
            print_error("Unknown argument %s", argv[i]);
            return -1;
        }
    }
    
    // Validate required arguments
    if (!args->image_name) {
        print_error("--image is required");
        return -1;
    }
    
    // Validate ranges
    if (args->size_kib < MIN_SIZE_KIB || args->size_kib > MAX_SIZE_KIB) {
        print_error("--size-kib must be between %d and %d", MIN_SIZE_KIB, MAX_SIZE_KIB);
        return -1;
    }
    
    if (args->inode_count < MIN_INODES || args->inode_count > MAX_INODES) {
        print_error("--inodes must be between %d and %d", MIN_INODES, MAX_INODES);
        return -1;
    }
    
    // Validate size alignment
    if (args->size_kib % 4 != 0) {
        print_error("--size-kib must be a multiple of 4");
        return -1;
    }
    
    // Validate that inode count is reasonable for the given size
    uint64_t total_blocks = (uint64_t)(args->size_kib * 1024) / BS;
    uint32_t inodes_per_block = BS / INODE_SIZE;
    uint64_t max_inodes = total_blocks * inodes_per_block;
    
    if (args->inode_count > max_inodes) {
        print_error("Too many inodes for the given size (max %lu)", max_inodes);
        return -1;
    }
    
    return 0;
}

// Calculate file system layout
int calculate_layout(const cli_args_builder_t* args, fs_layout_t* layout) {

    layout->total_blocks = (uint64_t)(args->size_kib * 1024) / BS;
    uint32_t inodes_per_block = BS / INODE_SIZE;  
    layout->inode_table_blocks = (args->inode_count + inodes_per_block - 1) / inodes_per_block;
    
    // Fixed layout positions
    layout->superblock_start = 0;
    layout->inode_bitmap_start = 1;
    layout->data_bitmap_start = 2;
    layout->inode_table_start = 3;
    layout->data_region_start = 3 + layout->inode_table_blocks;
    
    // Calculate data region blocks
    layout->data_region_blocks = layout->total_blocks - layout->data_region_start;
    
    // Validate space
    if (layout->data_region_blocks <= 0) {
        print_error("Not enough space for data region");
        return -1;
    }
    
    if (layout->data_region_blocks < 1) {
        print_error("Need at least 1 data block for root directory");
        return -1;
    }
    
    return 0;
}

// Create superblock
void create_superblock(superblock_t* sb, const cli_args_builder_t* args, const fs_layout_t* layout) {
    memset(sb, 0, sizeof(superblock_t));
    
    time_t now = time(NULL);
    
    sb->magic = MAGIC_NUMBER;  // "MVSF"
    sb->version = VERSION;
    sb->block_size = BS;
    sb->total_blocks = layout->total_blocks;
    sb->inode_count = args->inode_count;
    sb->inode_bitmap_start = layout->inode_bitmap_start;
    sb->inode_bitmap_blocks = 1;
    sb->data_bitmap_start = layout->data_bitmap_start;
    sb->data_bitmap_blocks = 1;
    sb->inode_table_start = layout->inode_table_start;
    sb->inode_table_blocks = layout->inode_table_blocks;
    sb->data_region_start = layout->data_region_start;
    sb->data_region_blocks = layout->data_region_blocks;
    sb->root_inode = ROOT_INO;
    sb->mtime_epoch = (uint64_t)now;
    sb->flags = 0;

    superblock_crc_finalize(sb);
}

// Create root inode
void create_root_inode(inode_t* root_inode, uint32_t first_data_block) {
    memset(root_inode, 0, sizeof(inode_t));
    
    time_t now = time(NULL);
    
    root_inode->mode = MODE_DIR;  
    root_inode->links = 2;       // . & .. 
    root_inode->uid = 0;
    root_inode->gid = 0;
    root_inode->size_bytes = 2 * sizeof(dirent64_t);  // . & ..
    root_inode->atime = (uint64_t)now;
    root_inode->mtime = (uint64_t)now;
    root_inode->ctime = (uint64_t)now;
    root_inode->direct[0] = first_data_block;  
    for (int i = 1; i < DIRECT_MAX; i++) {
        root_inode->direct[i] = 0;  // Unused blocks
    }
    root_inode->reserved_0 = 0;
    root_inode->reserved_1 = 0;
    root_inode->reserved_2 = 0;
    root_inode->proj_id = PROJ_ID; 
    root_inode->uid16_gid16 = 0;
    root_inode->xattr_ptr = 0;
    
    inode_crc_finalize(root_inode);
}

// Create root directory entries . & ..
void create_root_directory_entries(dirent64_t* dot_entry, dirent64_t* dotdot_entry) {
    // "." entry
    memset(dot_entry, 0, sizeof(dirent64_t));
    dot_entry->inode_no = ROOT_INO;
    dot_entry->type = FILE_TYPE_DIRECTORY;  
    strcpy(dot_entry->name, ".");
    dirent_checksum_finalize(dot_entry);  
    
    // ".." entry
    memset(dotdot_entry, 0, sizeof(dirent64_t));
    dotdot_entry->inode_no = ROOT_INO;
    dotdot_entry->type = FILE_TYPE_DIRECTORY;  
    strcpy(dotdot_entry->name, "..");
    dirent_checksum_finalize(dotdot_entry);  
}

// Initialize bitmaps
void initialize_bitmaps(uint8_t* inode_bitmap, uint8_t* data_bitmap, uint32_t inode_count, uint64_t data_blocks) {
    (void)inode_count;  // Suppress unused parameter warning
    (void)data_blocks;  // Suppress unused parameter warning
    
    memset(inode_bitmap, 0, BS);
    memset(data_bitmap, 0, BS);
    
    // Mark root inode (inode #1) as used
    inode_bitmap[0] |= 0x01;
    
    // Mark first data block as used (for root directory)
    data_bitmap[0] |= 0x01;
}

int main(int argc, char* argv[]) {
    crc32_init();
    
    // Parse command line arguments
    cli_args_builder_t args;
    if (parse_cli_args(argc, argv, &args) != 0) {
        return 1;
    }
    
    fs_layout_t layout;
    if (calculate_layout(&args, &layout) != 0) {
        return 1;
    }
    
    FILE* img_file = fopen(args.image_name, "wb");
    if (!img_file) {
        print_error("Cannot create image file %s: %s", args.image_name, strerror(errno));
        return 1;
    }
    
    // Create superblock
    superblock_t superblock;
    create_superblock(&superblock, &args, &layout);
    
    uint8_t block_buffer[BS];
    memset(block_buffer, 0, BS);
    memcpy(block_buffer, &superblock, sizeof(superblock_t));
    if (fwrite(block_buffer, BS, 1, img_file) != 1) {
        print_error("Error writing superblock");
        fclose(img_file);
        return 1;
    }
    
    // Initialize bitmaps
    uint8_t inode_bitmap[BS];
    uint8_t data_bitmap[BS];
    initialize_bitmaps(inode_bitmap, data_bitmap, args.inode_count, layout.data_region_blocks);
    
    if (fwrite(inode_bitmap, BS, 1, img_file) != 1) {
        print_error("Error writing inode bitmap");
        fclose(img_file);
        return 1;
    }
    
    // Write data bitmap
    if (fwrite(data_bitmap, BS, 1, img_file) != 1) {
        print_error("Error writing data bitmap");
        fclose(img_file);
        return 1;
    }
    
    // Create inode table
    for (uint64_t block = 0; block < layout.inode_table_blocks; block++) {
        memset(block_buffer, 0, BS);
        
        if (block == 0) {
            // First block contains root inode
            inode_t root_inode;
            uint32_t first_data_block = (uint32_t)layout.data_region_start;
            create_root_inode(&root_inode, first_data_block);
            memcpy(block_buffer, &root_inode, sizeof(inode_t));
        }
        
        if (fwrite(block_buffer, BS, 1, img_file) != 1) {
            print_error("Error writing inode table block %" PRIu64, block);
            fclose(img_file);
            return 1;
        }
    }
    
    // Write data region
    for (uint64_t block = 0; block < layout.data_region_blocks; block++) {
        memset(block_buffer, 0, BS);
        
        if (block == 0) {
            // First data block contains root directory entries
            dirent64_t dot_entry, dotdot_entry;
            create_root_directory_entries(&dot_entry, &dotdot_entry);
            memcpy(block_buffer, &dot_entry, sizeof(dirent64_t));
            memcpy(block_buffer + sizeof(dirent64_t), &dotdot_entry, sizeof(dirent64_t));
        }
        
        if (fwrite(block_buffer, BS, 1, img_file) != 1) {
            print_error("Error writing data block %" PRIu64, block);
            fclose(img_file);
            return 1;
        }
    }
    
    fclose(img_file);
    printf("Successfully created image: %s\n", args.image_name);
    
    return 0;
}
