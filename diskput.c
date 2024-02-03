#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

typedef struct __attribute__((packed)) {
    char fs_id[8];
    uint16_t block_size;
    uint32_t block_count;
    uint32_t fat_start;
    uint32_t fat_blocks;
    uint32_t root_dir_start;
    uint32_t root_dir_blocks;
} superblock_t;

typedef struct __attribute__((packed)) {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} dir_entry_timedate_t;

typedef struct __attribute__((packed)) {
    uint8_t status;
    uint32_t starting_block;
    uint32_t block_count;
    uint32_t size;
    dir_entry_timedate_t create_time;
    dir_entry_timedate_t modify_time;
    char filename[31];
    char unused[6];
} dir_entry_t;

void set_superblock_info(superblock_t *super_block) {
    super_block->block_size = ntohs(super_block->block_size);
    super_block->block_count = ntohl(super_block->block_count);
    super_block->fat_start = ntohl(super_block->fat_start);
    super_block->fat_blocks = ntohl(super_block->fat_blocks);
    super_block->root_dir_start = ntohl(super_block->root_dir_start);
    super_block->root_dir_blocks = ntohl(super_block->root_dir_blocks);
}

uint32_t find_free_block(FILE *file, const superblock_t *super_block) {
    fseek(file, super_block->fat_start * super_block->block_size, SEEK_SET);
    uint32_t fat;
    for (uint32_t i = 0; i < super_block->fat_blocks * super_block->block_size / sizeof(uint32_t); i++) {
        fread(&fat, sizeof(uint32_t), 1, file);
        fat = ntohl(fat); 
        if (fat == 0x00000000) { 
            return i; 
        }
    }
    return 0xFFFFFFFF;
}

void prepare_new_directory_entry(dir_entry_t *entry, const char *filename, FILE *source, FILE *file, superblock_t *super_block) {
    memset(entry, 0, sizeof(dir_entry_t));
    entry->status = 0x03;
    strncpy(entry->filename, filename, sizeof(entry->filename) - 1);
    entry->filename[sizeof(entry->filename) - 1] = '\0';

    fseek(source, 0, SEEK_END);
    entry->size = htonl(ftell(source));
    fseek(source, 0, SEEK_SET);

    time_t temp_time;
    struct tm *time_info;
    time(&temp_time);
    time_info = localtime(&temp_time);

    entry->create_time.year = htons(time_info->tm_year + 1900);
    entry->create_time.month = time_info->tm_mon + 1;
    entry->create_time.day = time_info->tm_mday;
    entry->create_time.hour = time_info->tm_hour;
    entry->create_time.minute = time_info->tm_min;
    entry->create_time.second = time_info->tm_sec;
    entry->modify_time = entry->create_time;

    uint32_t free_block = find_free_block(file, super_block);
    if (free_block == 0xFFFFFFFF) {
        //do nothing
    } else {
        entry->starting_block = htonl(free_block);
    }
}

bool update_fat(FILE *file, superblock_t *super_block, uint32_t start_block, uint32_t block_count) {
    uint32_t current_block = start_block;
    uint32_t next_block;

    for (uint32_t i = 0; i < block_count; i++) {
        fseek(file, (super_block->fat_start * super_block->block_size) + (current_block * sizeof(uint32_t)), SEEK_SET);
        
        if (i == block_count - 1) {
            next_block = htonl(0xFFFFFFFF);
        } else {
            next_block = htonl(find_free_block(file, super_block));
            if (next_block == htonl(0xFFFFFFFF)) {
                return false;
            }
        }

        fwrite(&next_block, sizeof(uint32_t), 1, file);
        current_block = ntohl(next_block);
    }
    return true;
}

bool add_file_to_directory(FILE *file, superblock_t *super_block, const dir_entry_t *new_entry, const char *path) {
    fseek(file, super_block->root_dir_start * super_block->block_size, SEEK_SET);
    dir_entry_t entry;
    for (uint32_t i = 0; i < super_block->root_dir_blocks * super_block->block_size / sizeof(dir_entry_t); i++) {
        fread(&entry, sizeof(dir_entry_t), 1, file);
        if (entry.status == 0x00 || entry.status == 0xFF) {
            fseek(file, -((long)sizeof(dir_entry_t)), SEEK_CUR);
            fwrite(new_entry, sizeof(dir_entry_t), 1, file);
            return true;
        }
    }
    return false;
}

bool copy_file_to_sfs(FILE *source, FILE *file, superblock_t *super_block, uint32_t start_block, uint32_t file_size) {
    fseek(file, start_block * super_block->block_size, SEEK_SET);
    char buffer[1024];
    uint32_t bytes_copied = 0;
    size_t bytes_to_copy;

    while (bytes_copied < file_size) {
        bytes_to_copy = sizeof(buffer);
        if (file_size - bytes_copied < sizeof(buffer)) {
            bytes_to_copy = file_size - bytes_copied;
        }
        fread(buffer, 1, bytes_to_copy, source);
        fwrite(buffer, 1, bytes_to_copy, file);
        bytes_copied += bytes_to_copy;
    }

    return true;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        perror("Usage: diskput <file system image> <source file> <destination path>\n");
        return 0;
    }

    FILE *file = fopen(argv[1], "r+b");
    FILE *source = fopen(argv[2], "rb");
    if (file == NULL || source == NULL) {
        printf("File not found.\n");
        exit(EXIT_FAILURE);
    }

    superblock_t super_block;
    if (fread(&super_block, sizeof(superblock_t), 1, file) != 1) {
        perror("Error reading superblock.\n");
        return 0;
    }

    set_superblock_info(&super_block);

    dir_entry_t entry;
    prepare_new_directory_entry(&entry, argv[2], source, file, &super_block);

    if (!add_file_to_directory(file, &super_block, &entry, argv[3])) {
        perror("Failed to add file to directory.\n");
        return 0;
    }

    if (!copy_file_to_sfs(source, file, &super_block, ntohl(entry.starting_block), ntohl(entry.size))) {
        perror("Failed to copy file to SFS.\n");
        return 0;
    }

    fclose(file);
    fclose(source);
    return EXIT_SUCCESS;
}