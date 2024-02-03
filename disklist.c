#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>
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
    uint8_t filename[31];
    uint8_t unused[6];
} dir_entry_t;

void find_subdirectories(const char *path, char segments[10][32], int *num_segments) {
    char *cur;
    char temp_path[256];
    strncpy(temp_path, path, sizeof(temp_path) - 1);
    temp_path[sizeof(temp_path) - 1] = '\0';
    *num_segments = 0;
    cur = strtok(temp_path, "/");

    while (cur != NULL && *num_segments < 10) {
        strncpy(segments[*num_segments], cur, 31);
        segments[*num_segments][31] = '\0';
        (*num_segments)++;
        cur = strtok(NULL, "/");
    }
}

bool list_directories(FILE *file, const superblock_t *super_block, uint32_t start_block, uint32_t block_count, const char subdirs[10][32], int cur_subdir, int num_subdirs) {
    dir_entry_t entry;
    bool result = false;
    long offset = (long)start_block * super_block->block_size;

    if (fseek(file, offset, SEEK_SET) == -1) {
        perror("Error seeking to directory.");
        return false;
    }
    for (int i = 0; i < block_count * (super_block->block_size / sizeof(dir_entry_t)); ++i) {
        if (fread(&entry, sizeof(dir_entry_t), 1, file) != 1) {
            perror("Error reading directory entry.");
            break;
        }
        if (entry.status == 0x00 || entry.status == 0xFF) {
            continue;
        }
        entry.filename[30] = '\0';
        if (cur_subdir < num_subdirs && (entry.status & 0x02) == 0) {
            if (strcmp(entry.filename, subdirs[cur_subdir]) == 0) {
                return list_directories(file, super_block, ntohl(entry.starting_block), ntohl(entry.block_count), subdirs, cur_subdir + 1, num_subdirs);
            }
        } else if (cur_subdir == num_subdirs) {
            printf("%c %10u %30s %04u/%02u/%02u %02u:%02u:%02u\n", (entry.status == 3) ? 'F' : 'D', ntohl(entry.size), entry.filename, ntohs(entry.modify_time.year), entry.modify_time.month, entry.modify_time.day, entry.modify_time.hour, entry.modify_time.minute, entry.modify_time.second);
            result = true;
        }
    }
    return result;
}

void set_superblock_info(superblock_t *super_block) {
    super_block->block_size = htons(super_block->block_size);
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
        return 1;
    }
    size_t read_status = fread(&super_block, sizeof(superblock_t), 1, file);
    if (read_status != 1) {
        fprintf(stderr, "Error reading superblock.\n");
        fclose(file);
        return 1;
    }
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Usage: disklist <file system image> <optional: path>\n");
        fclose(file);
        return 1;
    }

    set_superblock_info(&super_block);
    char subdirs[10][32];
    int num_subdirs = 0;
    if (argc == 3) {
        find_subdirectories(argv[2], subdirs, &num_subdirs);
    }

    bool result = list_directories(file, &super_block, super_block.root_dir_start, super_block.root_dir_blocks, subdirs, 0, num_subdirs);
    if (!result && num_subdirs > 0) {
        fprintf(stderr, "Subdirectory not found.\n");
    }

    fclose(file);
    return EXIT_SUCCESS;
}