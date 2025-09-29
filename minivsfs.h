#ifndef MINIVSFS_H
#define MINIVSFS_H

#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
#include <inttypes.h>
#include <assert.h>

// File system constants
#define BS 4096u               // Block size
#define INODE_SIZE 128u        // Inode size in bytes
#define ROOT_INO 1u           // Root inode number
#define DIRECT_MAX 12         // Maximum direct block pointers
#define MAGIC_NUMBER 0x4D565346  // "MVSF" magic number
#define VERSION 1             // File system version
#define PROJ_ID 7             // Project ID

// File type constants
#define FILE_TYPE_REGULAR 1
#define FILE_TYPE_DIRECTORY 2

// Mode constants
#define MODE_FILE 0100000     // Regular file mode
#define MODE_DIR 0040000      // Directory mode

// Validation constants
#define MIN_SIZE_KIB 180
#define MAX_SIZE_KIB 4096
#define MIN_INODES 128
#define MAX_INODES 512

// Packed structures for file system layout
#pragma pack(push, 1)

typedef struct {
    uint32_t magic;                   
    uint32_t version;                 
    uint32_t block_size;              // 4096
    uint64_t total_blocks;
    uint64_t inode_count;
    uint64_t inode_bitmap_start;      // block 1
    uint64_t inode_bitmap_blocks;     // 1
    uint64_t data_bitmap_start;       // block 2
    uint64_t data_bitmap_blocks;      // 1
    uint64_t inode_table_start;       // block 3
    uint64_t inode_table_blocks;
    uint64_t data_region_start;
    uint64_t data_region_blocks;
    uint64_t root_inode;              // 1
    uint64_t mtime_epoch;             
    uint32_t flags;                   // 0
    
    // THIS FIELD SHOULD STAY AT THE END
    // ALL OTHER FIELDS SHOULD BE ABOVE THIS
    uint32_t checksum;            // crc32(superblock[0..4091])
} superblock_t;

typedef struct {
    uint16_t mode;                    // 0100000 for files, 0040000 for directories
    uint16_t links;                   // Number of directories pointing to this
    uint32_t uid;                     // 0
    uint32_t gid;                     // 0
    uint64_t size_bytes;
    uint64_t atime;                   // Build time (Unix Epoch)
    uint64_t mtime;                   // Build time (Unix Epoch)
    uint64_t ctime;                   // Build time (Unix Epoch)
    uint32_t direct[12];              // Direct block pointers
    uint32_t reserved_0;              // 0
    uint32_t reserved_1;              // 0
    uint32_t reserved_2;              // 0
    uint32_t proj_id;                 // gpr 7
    uint32_t uid16_gid16;             // 0
    uint64_t xattr_ptr;               // 0

    // THIS FIELD SHOULD STAY AT THE END
    // ALL OTHER FIELDS SHOULD BE ABOVE THIS
    uint64_t inode_crc;   // low 4 bytes store crc32 of bytes [0..119]; high 4 bytes 0
} inode_t;

typedef struct {
    uint32_t inode_no;                // 0 if free
    uint8_t  type;                    // 1=file, 2=dir
    char     name[58];                // null-terminated filename
    
    uint8_t  checksum; // XOR of bytes 0..62
} dirent64_t;

#pragma pack(pop)

// Static assertions for structure sizes
_Static_assert(sizeof(superblock_t) == 116, "superblock must fit in one block");
_Static_assert(sizeof(inode_t) == INODE_SIZE, "inode size mismatch");
_Static_assert(sizeof(dirent64_t) == 64, "dirent size mismatch");

// Command line argument structures
typedef struct {
    char* input_image;
    char* output_image;
    char* filename;
} cli_args_adder_t;

typedef struct {
    char* image_name;
    uint32_t size_kib;
    uint32_t inode_count;
} cli_args_builder_t;

// File system layout structure
typedef struct {
    uint64_t total_blocks;
    uint64_t inode_table_blocks;
    uint64_t data_region_blocks;
    uint64_t superblock_start;        // 0
    uint64_t inode_bitmap_start;      // 1
    uint64_t data_bitmap_start;       // 2
    uint64_t inode_table_start;       // 3
    uint64_t data_region_start;       // 3 + inode_table_blocks
} fs_layout_t;

// CRC32 functions
extern uint32_t CRC32_TAB[256];
void crc32_init(void);
uint32_t crc32(const void* data, size_t n);

// Checksum functions
uint32_t superblock_crc_finalize(superblock_t *sb);
void inode_crc_finalize(inode_t* ino);
void dirent_checksum_finalize(dirent64_t* de);

// Utility functions
int find_free_bit(uint8_t* bitmap, uint32_t max_bits);
void set_bit(uint8_t* bitmap, int bit_number);
const char* extract_filename(const char* path);
uint8_t* read_file_content(const char* filename, uint64_t* file_size);

// Error handling
void print_error(const char* format, ...);
void cleanup_and_exit(int exit_code, ...);

#endif // MINIVSFS_H
