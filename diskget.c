#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdbool.h>

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

void find_subdirectories(const char *path, char subdirs[10][32], int *num_subdirs) {
    char *cur;
    char temp_path[100];
    strncpy(temp_path, path, sizeof(temp_path) - 1);
    temp_path[sizeof(temp_path) - 1] = '\0';
    *num_subdirs = 0;
    cur = strtok(temp_path, "/");
    while (cur != NULL) {
        strncpy(subdirs[*num_subdirs], cur, 31);
        subdirs[*num_subdirs][31] = '\0';
        (*num_subdirs)++;
        cur = strtok(NULL, "/");
    }
}

bool copy_file(FILE *file, uint32_t source_start_block, uint32_t source_size, const superblock_t *super_block, const char *dest_filename) {
    long src_offset = (long)source_start_block * super_block->block_size;
    if (fseek(file, src_offset, SEEK_SET) == -1) {
        perror("Error seeking to source.");
        return false;
    }
    FILE *dest_file = fopen(dest_filename, "wb");
    if (dest_file == NULL) {
        perror("Error opening destination file.");
        return false;
    }
    char buffer[super_block->block_size];
    uint32_t remaining_size = source_size;
    while (remaining_size > 0) {
        size_t read_size = (remaining_size < super_block->block_size) ? remaining_size : super_block->block_size;
        if (fread(buffer, 1, read_size, file) != read_size) {
            perror("Error reading source file.");
            fclose(dest_file);
            return false;
        }
        if (fwrite(buffer, 1, read_size, dest_file) != read_size) {
            perror("Error writing to destination file.");
            fclose(dest_file);
            return false;
        }
        remaining_size -= read_size;
    }
    fclose(dest_file);
    return true;
}

bool find_file_in_directory(FILE *file, const superblock_t *super_block, uint32_t start_block, uint32_t block_count, const char subdirs[10][32], int cur_subdir, int num_subdirs, const char *filename, uint32_t *src_start_block, uint32_t *src_size) {
    dir_entry_t entry;
    long offset = (long)start_block * super_block->block_size;
    fseek(file, offset, SEEK_SET);
    for (int i = 0; i < block_count * (super_block->block_size / sizeof(dir_entry_t)); ++i) {
        if (fread(&entry, sizeof(dir_entry_t), 1, file) != 1) {
            break;
        }
        if (entry.status == 0x00 || entry.status == 0xFF) {
            continue;
        }
        entry.filename[30] = '\0';
        if (cur_subdir < num_subdirs && (entry.status & 0x02) == 0) {
            if (strcmp(entry.filename, subdirs[cur_subdir]) == 0) {
                return find_file_in_directory(file, super_block, ntohl(entry.starting_block), ntohl(entry.block_count), subdirs, cur_subdir + 1, num_subdirs, filename, src_start_block, src_size);
            }
        } else if (cur_subdir + 1 == num_subdirs) {
            if (strcmp(entry.filename, filename) == 0) {
                *src_start_block = ntohl(entry.starting_block);
                *src_size = ntohl(entry.size);
                return true;
            }
        }
    }
    return false;
}

void set_superblock_info(superblock_t *super_block) {
    super_block->block_size = ntohs(super_block->block_size);
    super_block->block_count = ntohl(super_block->block_count);
    super_block->fat_start = ntohl(super_block->fat_start);
    super_block->fat_blocks = ntohl(super_block->fat_blocks);
    super_block->root_dir_start = ntohl(super_block->root_dir_start);
    super_block->root_dir_blocks = ntohl(super_block->root_dir_blocks);
}

int main(int argc, char *argv[]) {
    superblock_t super_block;
    FILE *file = fopen(argv[1], "rb");
    if (file == NULL) {
        fprintf(stderr, "Error opening file system image %s.\n", argv[1]);
        exit(EXIT_FAILURE);
    }
    size_t read_status = fread(&super_block, sizeof(superblock_t), 1, file);
    if (read_status != 1) {
        fprintf(stderr, "Error reading superblock from file system.\n");
        fclose(file);
        exit(EXIT_FAILURE);
    }
    if (argc != 4) {
        fprintf(stderr, "Usage: diskget <file system image> <path to file> <destination file>\n");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    char *file_after_subdir = argv[2];
    char *lastSlash = strrchr(file_after_subdir, '/');
    char *contents_chopped = lastSlash ? lastSlash + 1 : file_after_subdir;
    char subdirs[10][32];
    int num_subdirs;
    uint32_t src_start_block = 0;
    uint32_t src_size = 0;

    find_subdirectories(argv[2], subdirs, &num_subdirs);
    set_superblock_info(&super_block);

    bool file_exists = find_file_in_directory(file, &super_block, super_block.root_dir_start, super_block.root_dir_blocks, subdirs, 0, num_subdirs, contents_chopped, &src_start_block, &src_size);
    if (file_exists) {
        const char *dest_filename = argv[3];
        if (copy_file(file, src_start_block, src_size, &super_block, dest_filename)) {
            fclose(file);
            return EXIT_SUCCESS;
        } else {
            fprintf(stderr, "Error copying the file.\n");
        }
    } else {
        fprintf(stderr, "File not found.\n");
        fclose(file);
        return EXIT_SUCCESS;
    }
    
    fclose(file);
    return EXIT_SUCCESS;
}