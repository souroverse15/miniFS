#include "minivsfs.h"
#include <stdarg.h>

// CRC32 implementation
uint32_t CRC32_TAB[256];

void crc32_init(void) {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++) {
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        }
        CRC32_TAB[i] = c;
    }
}

uint32_t crc32(const void* data, size_t n) {
    const uint8_t* p = (const uint8_t*)data;
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) {
        c = CRC32_TAB[(c ^ p[i]) & 0xFF] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFFu;
}

// Checksum functions
uint32_t superblock_crc_finalize(superblock_t *sb) {
    sb->checksum = 0;
    uint32_t s = crc32((void *) sb, BS - 4);
    sb->checksum = s;
    return s;
}

void inode_crc_finalize(inode_t* ino) {
    uint8_t tmp[INODE_SIZE];
    memcpy(tmp, ino, INODE_SIZE);
    // Zero CRC area before computing
    memset(&tmp[120], 0, 8);
    uint32_t c = crc32(tmp, 120);
    ino->inode_crc = (uint64_t)c; // Low 4 bytes carry the CRC
}

void dirent_checksum_finalize(dirent64_t* de) {
    const uint8_t* p = (const uint8_t*)de;
    uint8_t x = 0;
    for (int i = 0; i < 63; i++) {
        x ^= p[i];   // Covers ino(4) + type(1) + name(58)
    }
    de->checksum = x;
}

// Utility functions
int find_free_bit(uint8_t* bitmap, uint32_t max_bits) {
    for (uint32_t byte_idx = 0; byte_idx < (max_bits + 7) / 8; byte_idx++) {
        if (bitmap[byte_idx] != 0xFF) {  // If a byte has at least one free bit
            for (int bit_idx = 0; bit_idx < 8; bit_idx++) {
                if ((bitmap[byte_idx] & (1 << bit_idx)) == 0) {
                    int bit_number = byte_idx * 8 + bit_idx;
                    if ((uint32_t)bit_number < max_bits) {
                        return bit_number;
                    }
                }
            }
        }
    }
    return -1;  // Return -1 if no free bit found
}

void set_bit(uint8_t* bitmap, int bit_number) {
    int byte_idx = bit_number / 8;
    int bit_idx = bit_number % 8;
    bitmap[byte_idx] |= (1 << bit_idx);
}

const char* extract_filename(const char* path) {
    const char* filename = strrchr(path, '/');
    if (filename) {
        return filename + 1;  
    }
    return path;  // No path separator found, return original
}

uint8_t* read_file_content(const char* filename, uint64_t* file_size) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        print_error("Cannot open file %s: %s", filename, strerror(errno));
        return NULL;
    }
    
    // Get file size
    if (fseek(file, 0, SEEK_END) != 0) {
        print_error("Cannot seek to end of file %s", filename);
        fclose(file);
        return NULL;
    }
    
    long size = ftell(file);
    if (size < 0) {
        print_error("Cannot get size of file %s", filename);
        fclose(file);
        return NULL;
    }
    
    *file_size = (uint64_t)size;
    
    if (fseek(file, 0, SEEK_SET) != 0) {
        print_error("Cannot seek to beginning of file %s", filename);
        fclose(file);
        return NULL;
    }
    
    // Allocate buffer and read content
    uint8_t* content = malloc(*file_size);
    if (!content) {
        print_error("Cannot allocate memory for file content");
        fclose(file);
        return NULL;
    }
    
    if (fread(content, 1, *file_size, file) != *file_size) {
        print_error("Cannot read file content");
        free(content);
        fclose(file);
        return NULL;
    }
    
    fclose(file);
    return content;
}

// Error handling functions
void print_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "Error: ");
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
}

void cleanup_and_exit(int exit_code, ...) {
    va_list args;
    va_start(args, exit_code);
    
    void* ptr;
    while ((ptr = va_arg(args, void*)) != NULL) {
        free(ptr);
    }
    
    va_end(args);
    exit(exit_code);
}
