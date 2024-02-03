#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

typedef struct __attribute__((packed)) {
    uint8_t fs_id[8];
    uint16_t block_size;
    uint32_t block_count;
    uint32_t fat_start;
    uint32_t fat_blocks;
    uint32_t root_dir_start;
    uint32_t root_dir_blocks;
} superblock_t;

uint32_t convert_endian(uint32_t value) {
    return ntohl(value);
}

uint32_t get_block_size(const superblock_t *super_block) {
    return htons(super_block->block_size);
}

uint32_t get_block_count(const superblock_t *super_block) {
    return ntohl(super_block->block_count);
}

uint32_t get_fat_start(const superblock_t *super_block) {
    return ntohl(super_block->fat_start);
}

uint32_t get_fat_blocks(const superblock_t *super_block) {
    return ntohl(super_block->fat_blocks);
}

uint32_t get_root_dir_start(const superblock_t *super_block) {
    return ntohl(super_block->root_dir_start);
}

uint32_t get_root_dir_blocks(const superblock_t *super_block) {
    return ntohl(super_block->root_dir_blocks);
}

int read_super_block(const char *file_path, superblock_t *super_block) {
    int file = open(file_path, O_RDWR);
    struct stat buffer;
    if (file == -1) {
        perror("Error opening file.");
        return -1;
    }
    if (read(file, super_block, sizeof(superblock_t)) != sizeof(superblock_t)) {
        perror("Error reading superblock.");
        close(file);
        return -1;
    }
    close(file);
    return 0;
}

int read_fat_info(const char *file_path, const superblock_t *super_block, uint32_t *free_blocks, uint32_t *reserved_blocks, uint32_t *allocated_blocks) {
    int file = open(file_path, O_RDONLY);
    if (file == -1) {
        perror("Error opening file.");
        return -1;
    }
    off_t fat_offset = (off_t)get_fat_start(super_block) * get_block_size(super_block);
    size_t fat_size = (size_t)get_fat_blocks(super_block) * get_block_size(super_block);
    uint8_t *fat = malloc(fat_size);

    if (fat == NULL) {
        perror("Error allocating memory for FAT.");
        close(file);
        return -1;
    }
    if (lseek(file, fat_offset, SEEK_SET) == -1) {
        perror("Error seeking to FAT.");
        close(file);
        free(fat);
        return -1;
    }

    if (lseek(file, fat_offset, SEEK_SET) == -1 || read(file, fat, fat_size) != fat_size) {
        perror("Error seeking to FAT.");
        close(file);
        free(fat);
        return -1;
    }

    lseek(file, fat_offset, SEEK_SET);

    *free_blocks = *reserved_blocks = *allocated_blocks = 0;

    for (size_t i = 0; i < fat_size / sizeof(uint32_t); ++i) {
        uint32_t entry = ntohl(((uint32_t *)fat)[i]);
        if (entry == 0 || entry == 0x00000000) {
            (*free_blocks)++;
        } else if (entry == 1 || entry == 0x00000001) {
            (*reserved_blocks)++;
        } else {
            (*allocated_blocks)++;
        }
    }

    close(file);
    free(fat);
    return 0;
}

void display_super_block_info(const superblock_t *super_block) {
    printf("Super block information\n");
    printf("Block size: %u\n", get_block_size(super_block));
    printf("Block count: %u\n", get_block_count(super_block));
    printf("FAT starts: %u\n", get_fat_start(super_block));
    printf("FAT blocks: %u\n", get_fat_blocks(super_block));
    printf("Root directory starts: %u\n", get_root_dir_start(super_block));
    printf("Root directory blocks: %u\n", get_root_dir_blocks(super_block));
}

void display_fat_info(uint32_t free_blocks, uint32_t reserved_blocks, uint32_t allocated_blocks) {
    printf("FAT information\n");
    printf("Free blocks: %u\n", free_blocks);
    printf("Reserved blocks: %u\n", reserved_blocks);
    printf("Allocated blocks: %u\n", allocated_blocks);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file_system_image>\n", argv[0]);
        return 1;
    }
    
    superblock_t super_block;
    if (read_super_block(argv[1], &super_block) != 0) {
        return 1;
    }

    display_super_block_info(&super_block);
    uint32_t free_blocks, reserved_blocks, allocated_blocks;
    if (read_fat_info(argv[1], &super_block, &free_blocks, &reserved_blocks, &allocated_blocks) != 0) {
        return 1;
    }
    printf("\n");
    display_fat_info(free_blocks, reserved_blocks, allocated_blocks);
    return 0;
}