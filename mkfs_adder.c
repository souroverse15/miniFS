// Build: gcc -O2 -std=c17 -Wall -Wextra mkfs_adder.c minivsfs_utils.c -o mkfs_adder
#include "minivsfs.h"

// Parse command line arguments
int parse_cli_args(int argc, char* argv[], cli_args_adder_t* args) {
    args->input_image = NULL;
    args->output_image = NULL;
    args->filename = NULL;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--input") == 0) {
            if (i + 1 >= argc) {
                print_error("--input requires a filename");
                return -1;
            }
            args->input_image = argv[++i];
        }
        else if (strcmp(argv[i], "--output") == 0) {
            if (i + 1 >= argc) {
                print_error("--output requires a filename");
                return -1;
            }
            args->output_image = argv[++i];
        }
        else if (strcmp(argv[i], "--file") == 0) {
            if (i + 1 >= argc) {
                print_error("--file requires a filename");
                return -1;
            }
            args->filename = argv[++i];
        }
        else {
            print_error("Unknown argument %s", argv[i]);
            return -1;
        }
    }
    
    if (!args->input_image) {
        print_error("--input is required");
        return -1;
    }
    
    if (!args->output_image) {
        print_error("--output is required");
        return -1;
    }
    
    if (!args->filename) {
        print_error("--file is required");
        return -1;
    }
    
    // Validate filename length
    const char* filename_only = extract_filename(args->filename);
    if (strlen(filename_only) > 57) {
        print_error("Filename too long (max 57 characters)");
        return -1;
    }
    
    return 0;
}


int main(int argc, char* argv[]) {
    crc32_init();
    
    // Parse CLI arguments
    cli_args_adder_t args;
    if (parse_cli_args(argc, argv, &args) != 0) {
        return 1;
    }
    
    // Check if the file to be added exists
    uint64_t file_size;
    uint8_t* file_content = read_file_content(args.filename, &file_size);
    if (!file_content) {
        return 1;
    }
    
    // Validate file size
    if (file_size == 0) {
        print_error("File is empty");
        free(file_content);
        return 1;
    }
    
    // Calculate blocks needed for file
    uint64_t blocks_needed = (file_size + BS - 1) / BS;  // Round up
    if (blocks_needed > DIRECT_MAX) {
        print_error("File too large (needs %lu blocks, max %d)", blocks_needed, DIRECT_MAX);
        free(file_content);
        return 1;
    }
    
    // Open input image
    FILE* input_file = fopen(args.input_image, "rb");
    if (!input_file) {
        print_error("Cannot open input image %s: %s", args.input_image, strerror(errno));
        free(file_content);
        return 1;
    }
    
    // Read the superblock of the image
    superblock_t sb;
    uint8_t block_buffer[BS];
    if (fread(block_buffer, BS, 1, input_file) != 1) {
        print_error("Cannot read superblock");
        fclose(input_file);
        free(file_content);
        return 1;
    }
    memcpy(&sb, block_buffer, sizeof(superblock_t));
    
    // Validate magic number
    if (sb.magic != MAGIC_NUMBER) {
        print_error("Invalid file system magic number");
        fclose(input_file);
        free(file_content);
        return 1;
    }
    
    // Read inode bitmap
    uint8_t inode_bitmap[BS];
    if (fread(inode_bitmap, BS, 1, input_file) != 1) {
        print_error("Cannot read inode bitmap");
        fclose(input_file);
        free(file_content);
        return 1;
    }
    
    // Read data bitmap
    uint8_t data_bitmap[BS];
    if (fread(data_bitmap, BS, 1, input_file) != 1) {
        print_error("Cannot read data bitmap");
        fclose(input_file);
        free(file_content);
        return 1;
    }
    
    // Locate free inode
    int free_inode_bit = find_free_bit(inode_bitmap, (uint32_t)sb.inode_count);
    if (free_inode_bit < 0) {
        print_error("No free inodes available");
        fclose(input_file);
        free(file_content);
        return 1;
    }
    uint32_t new_inode_num = free_inode_bit + 1;  // Inodes are 1-indexed
    
    // Locate free data blocks
    uint32_t data_blocks[DIRECT_MAX];
    for (uint64_t i = 0; i < blocks_needed; i++) {
        int free_data_bit = find_free_bit(data_bitmap, (uint32_t)sb.data_region_blocks);
        if (free_data_bit < 0) {
            print_error("Not enough free data blocks (need %lu)", blocks_needed);
            fclose(input_file);
            free(file_content);
            return 1;
        }
        data_blocks[i] = (uint32_t)sb.data_region_start + free_data_bit;
        set_bit(data_bitmap, free_data_bit);
    }
    
    // Mark inode as used
    set_bit(inode_bitmap, free_inode_bit);
    
    // Read inode table
    uint8_t* inode_table = malloc(sb.inode_table_blocks * BS);
    if (!inode_table) {
        print_error("Cannot allocate memory for inode table");
        fclose(input_file);
        free(file_content);
        return 1;
    }
    
    if (fread(inode_table, BS, sb.inode_table_blocks, input_file) != sb.inode_table_blocks) {
        print_error("Cannot read inode table");
        fclose(input_file);
        free(file_content);
        free(inode_table);
        return 1;
    }
    
    // Create new inode for the file
    inode_t* new_inode = (inode_t*)(inode_table + (new_inode_num - 1) * INODE_SIZE);
    memset(new_inode, 0, sizeof(inode_t));
    
    time_t now = time(NULL);
    new_inode->mode = MODE_FILE;  // File mode
    new_inode->links = 1;
    new_inode->uid = 0;
    new_inode->gid = 0;
    new_inode->size_bytes = file_size;
    new_inode->atime = (uint64_t)now;
    new_inode->mtime = (uint64_t)now;
    new_inode->ctime = (uint64_t)now;
    
    // Set the direct block pointers
    for (uint64_t i = 0; i < blocks_needed; i++) {
        new_inode->direct[i] = data_blocks[i];
    }
    for (uint64_t i = blocks_needed; i < DIRECT_MAX; i++) {
        new_inode->direct[i] = 0;
    }
    
    new_inode->reserved_0 = 0;
    new_inode->reserved_1 = 0;
    new_inode->reserved_2 = 0;
    new_inode->proj_id = PROJ_ID;
    new_inode->uid16_gid16 = 0;
    new_inode->xattr_ptr = 0;
    
    inode_crc_finalize(new_inode);
    
    // Update root directory
    inode_t* root_inode = (inode_t*)(inode_table + (ROOT_INO - 1) * INODE_SIZE);
    root_inode->links++;  // Increment link count
    root_inode->mtime = (uint64_t)now;
    inode_crc_finalize(root_inode);
    
    // Read data region to update root directory
    uint8_t* data_region = malloc(sb.data_region_blocks * BS);
    if (!data_region) {
        print_error("Cannot allocate memory for data region");
        fclose(input_file);
        free(file_content);
        free(inode_table);
        return 1;
    }
    
    if (fread(data_region, BS, sb.data_region_blocks, input_file) != sb.data_region_blocks) {
        print_error("Cannot read data region");
        fclose(input_file);
        free(file_content);
        free(inode_table);
        free(data_region);
        return 1;
    }
    
    fclose(input_file);
    
    // Find free directory entry in root directory
    uint32_t root_data_block = root_inode->direct[0] - (uint32_t)sb.data_region_start;
    uint8_t* root_dir_data = data_region + root_data_block * BS;
    dirent64_t* entries = (dirent64_t*)root_dir_data;
    
    // Find first free entry (skip . & .. at pos 0 and 1)
    int free_entry_idx = -1;
    int max_entries = (int)(BS / sizeof(dirent64_t));
    for (int i = 2; i < max_entries; i++) {
        if (entries[i].inode_no == 0) {
            free_entry_idx = i;
            break;
        }
    }
    
    if (free_entry_idx < 0) {
        print_error("No free directory entries in root directory");
        free(file_content);
        free(inode_table);
        free(data_region);
        return 1;
    }
    
    // Create new directory entry
    dirent64_t* new_entry = &entries[free_entry_idx];
    memset(new_entry, 0, sizeof(dirent64_t));
    new_entry->inode_no = new_inode_num;
    new_entry->type = FILE_TYPE_REGULAR;  // File
    const char* filename_only = extract_filename(args.filename);
    strncpy(new_entry->name, filename_only, 57);  
    new_entry->name[57] = '\0';  
    dirent_checksum_finalize(new_entry);
    
    // Update root inode size
    root_inode->size_bytes += sizeof(dirent64_t);
    inode_crc_finalize(root_inode);
    
    // Write file content to data blocks
    for (uint64_t i = 0; i < blocks_needed; i++) {
        uint32_t block_offset = data_blocks[i] - (uint32_t)sb.data_region_start;
        uint8_t* block_data = data_region + block_offset * BS;
        
        uint64_t bytes_to_copy = (i == blocks_needed - 1) ? 
            (file_size - i * BS) : BS;
        
        memset(block_data, 0, BS);  // Clear block first
        memcpy(block_data, file_content + i * BS, bytes_to_copy);
    }
    
    free(file_content);
    
    // Update superblock timestamp
    sb.mtime_epoch = (uint64_t)now;
    superblock_crc_finalize(&sb);
    
    // Write the updated file system
    FILE* output_file = fopen(args.output_image, "wb");
    if (!output_file) {
        print_error("Cannot create output image %s: %s", args.output_image, strerror(errno));
        free(inode_table);
        free(data_region);
        return 1;
    }
    
    // Write superblock
    memset(block_buffer, 0, BS);
    memcpy(block_buffer, &sb, sizeof(superblock_t));
    if (fwrite(block_buffer, BS, 1, output_file) != 1) {
        print_error("Cannot write superblock");
        fclose(output_file);
        free(inode_table);
        free(data_region);
        return 1;
    }
    
    // Write inode bitmap
    if (fwrite(inode_bitmap, BS, 1, output_file) != 1) {
        print_error("Cannot write inode bitmap");
        fclose(output_file);
        free(inode_table);
        free(data_region);
        return 1;
    }
    
    // Write data bitmap
    if (fwrite(data_bitmap, BS, 1, output_file) != 1) {
        print_error("Cannot write data bitmap");
        fclose(output_file);
        free(inode_table);
        free(data_region);
        return 1;
    }
    
    // Write inode table
    if (fwrite(inode_table, BS, sb.inode_table_blocks, output_file) != sb.inode_table_blocks) {
        print_error("Cannot write inode table");
        fclose(output_file);
        free(inode_table);
        free(data_region);
        return 1;
    }
    
    // Write data region
    if (fwrite(data_region, BS, sb.data_region_blocks, output_file) != sb.data_region_blocks) {
        print_error("Cannot write data region");
        fclose(output_file);
        free(inode_table);
        free(data_region);
        return 1;
    }
    
    fclose(output_file);
    free(inode_table);
    free(data_region);
    
    printf("Successfully added file '%s' to %s as %s\n", args.filename, args.output_image, filename_only);
    printf("Assigned inode: %u\n", new_inode_num);
    
    return 0;
}
