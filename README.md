# miniFS

A lightweight, educational file system implementation in C with complete toolchain for creating and managing file system images.

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C Standard](https://img.shields.io/badge/C-C17-blue.svg)](https://en.wikipedia.org/wiki/C17_(C_standard_revision))
[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)]()

## 🚀 Overview

miniFS is a simple, yet complete file system implementation designed for educational purposes and understanding the fundamentals of file system design. It features:

- **Fixed-size blocks** (4096 bytes)
- **Inode-based** file management
- **Direct block pointers** (up to 12 blocks per file)
- **CRC32 checksums** for data integrity
- **Bitmap allocation** for inodes and data blocks
- **Complete toolchain** for image creation and file management

## 📁 Architecture

```
Block Layout:
┌─────────────┬─────────────┬─────────────┬─────────────┬─────────────┐
│ Superblock  │ Inode Bitmap│ Data Bitmap │ Inode Table │ Data Region │
│   (Block 0) │   (Block 1) │   (Block 2) │  (Block 3+) │  (Remaining)│
└─────────────┴─────────────┴─────────────┴─────────────┴─────────────┘
```

### Key Components

- **Superblock**: Contains file system metadata, layout information, and integrity checksums
- **Inode Bitmap**: Tracks allocation status of inodes
- **Data Bitmap**: Tracks allocation status of data blocks
- **Inode Table**: Stores file/directory metadata and block pointers
- **Data Region**: Contains actual file data and directory entries

## 🛠️ Building

### Prerequisites
- GCC compiler with C17 support
- Make utility
- POSIX-compliant system (Linux, macOS, Unix)

### Quick Start
```bash
# Clone the repository
git clone https://github.com/souroverse15/miniFS.git
cd miniFS

# Build the project
make

# Run tests
make test
```

### Build Targets
```bash
make all        # Build all executables (default)
make clean      # Remove build artifacts
make test       # Run automated tests
make install    # Install to /usr/local/bin (requires sudo)
make uninstall  # Remove from /usr/local/bin (requires sudo)
make help       # Show available targets
```

## 📚 Usage

### Creating a File System Image

```bash
./mkfs_builder --image <filename> --size-kib <size> --inodes <count>
```

**Parameters:**
- `--image`: Output image filename
- `--size-kib`: Size in KiB (180-4096, must be multiple of 4)
- `--inodes`: Number of inodes (128-512)

**Example:**
```bash
./mkfs_builder --image filesystem.img --size-kib 1024 --inodes 256
```

### Adding Files to an Image

```bash
./mkfs_adder --input <input_image> --output <output_image> --file <filename>
```

**Parameters:**
- `--input`: Input file system image
- `--output`: Output file system image (can be same as input)
- `--file`: File to add to the image

**Example:**
```bash
./mkfs_adder --input filesystem.img --output filesystem.img --file document.txt
```

## 📋 Examples

### Complete Workflow
```bash
# 1. Create a new file system
./mkfs_builder --image myfs.img --size-kib 2048 --inodes 512

# 2. Add some files
echo "Hello, miniFS!" > greeting.txt
echo "This is a test file" > test.txt

./mkfs_adder --input myfs.img --output myfs.img --file greeting.txt
./mkfs_adder --input myfs.img --output myfs.img --file test.txt

# 3. The file system now contains both files
```

### Automated Testing
```bash
# Run the built-in test suite
make test

# Output:
# Running basic tests...
# Creating test filesystem...
# Successfully created image: test.img
# Adding test file...
# Successfully added file 'test.txt' to test_with_file.img as test.txt
# Assigned inode: 2
# Cleaning up test files...
```

## 🏗️ File Structure

```
miniFS/
├── README.md           # This file
├── Makefile           # Build configuration
├── .gitignore         # Git ignore patterns
├── minivsfs.h         # Common header with data structures
├── minivsfs_utils.c   # Shared utility functions
├── mkfs_builder.c     # File system creation tool
└── mkfs_adder.c       # File addition tool
```

## 📊 Data Structures

### Superblock (116 bytes)
```c
typedef struct {
    uint32_t magic;                 // Magic number (0x4D565346)
    uint32_t version;               // File system version
    uint32_t block_size;            // Block size (4096 bytes)
    uint64_t total_blocks;          // Total number of blocks
    uint64_t inode_count;           // Number of inodes
    // ... layout information
    uint32_t checksum;              // CRC32 checksum
} superblock_t;
```

### Inode (128 bytes)
```c
typedef struct {
    uint16_t mode;                  // File type and permissions
    uint16_t links;                 // Link count
    uint64_t size_bytes;            // File size in bytes
    uint64_t atime, mtime, ctime;   // Timestamps
    uint32_t direct[12];            // Direct block pointers
    uint64_t inode_crc;             // CRC32 checksum
} inode_t;
```

### Directory Entry (64 bytes)
```c
typedef struct {
    uint32_t inode_no;              // Inode number (0 if free)
    uint8_t type;                   // File type (1=file, 2=dir)
    char name[58];                  // Filename (null-terminated)
    uint8_t checksum;               // XOR checksum
} dirent64_t;
```

## ⚡ Features

### ✅ Implemented
- [x] Fixed-size block allocation
- [x] Inode-based file metadata
- [x] Direct block pointers (12 per file)
- [x] Root directory with . and .. entries
- [x] CRC32 data integrity checking
- [x] Bitmap-based allocation tracking
- [x] Complete command-line toolchain
- [x] Comprehensive error handling
- [x] Automated testing framework

### 🚧 Limitations
- Maximum file size: 48 KB (12 × 4KB blocks)
- Maximum filename length: 57 characters
- No subdirectories (flat structure)
- No indirect block pointers
- No symbolic links or special files
- No file permissions beyond basic mode

### 🔮 Future Enhancements
- [ ] Indirect block pointers for larger files
- [ ] Subdirectory support
- [ ] File permissions and ownership
- [ ] Symbolic links
- [ ] Block group organization
- [ ] Journal support

## 🔍 Technical Details

### Allocation Strategy
- **First-fit allocation** for both inodes and data blocks
- **Bitmap tracking** for efficient free space management
- **Contiguous allocation** preferred for file data

### Integrity Guarantees
- **CRC32 checksums** for superblock and inodes
- **XOR checksums** for directory entries
- **Magic number validation** for file system detection
- **Bounds checking** for all operations

### Error Handling
- Comprehensive input validation
- Graceful error recovery
- Detailed error messages
- Memory leak prevention

## 🧪 Testing

The project includes comprehensive testing:

```bash
# Automated test suite
make test

# Manual testing examples
./mkfs_builder --image test.img --size-kib 512 --inodes 128
echo "Test content" > sample.txt
./mkfs_adder --input test.img --output test.img --file sample.txt
```

## 🤝 Contributing

Contributions are welcome! Please follow these guidelines:

1. **Fork** the repository
2. **Create** a feature branch (`git checkout -b feature/amazing-feature`)
3. **Commit** your changes (`git commit -m 'Add amazing feature'`)
4. **Push** to the branch (`git push origin feature/amazing-feature`)
5. **Open** a Pull Request

### Development Setup
```bash
git clone https://github.com/souroverse15/miniFS.git
cd miniFS
make clean && make
make test
```

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 👥 Authors

- **Sourove Ahmed** - *Initial work* - [@souroverse15](https://github.com/souroverse15)

## 🙏 Acknowledgments

- Inspired by classic Unix file systems
- Educational resource for understanding file system internals
- Built with modern C practices and comprehensive error handling

## 📞 Support

If you have questions or need help:

1. Check the [Issues](https://github.com/souroverse15/miniFS/issues) page
2. Create a new issue with detailed information
3. Review the documentation and examples above

---

**miniFS** - Making file systems accessible for learning and experimentation! 🚀
