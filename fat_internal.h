#ifndef FAT_INTERNAL_H
#define FAT_INTERNAL_H

#include "fat.h"

extern unsigned char sector_buffer[SECTOR_SIZE];
extern PartitionTable pt[4];
extern Fat16BootSector bs;
extern unsigned short cwd_cluster;
extern unsigned int fat_start_lba;
extern unsigned int root_start_lba;
extern unsigned int root_dir_sectors;
extern unsigned int data_start_lba;

void *k_memcpy(void *dest, const void *src, unsigned int n);
unsigned int k_strlen(const char *str);
void print_string(const char *s);
void print_dec(unsigned int value);
void print_2d(unsigned int v);
void print_dec_width(unsigned int v, unsigned int w);

int str_eq(const char *a, const char *b);
int parse_path_component(const char **path, char *component, unsigned int max_len);

int read_sector(unsigned int sector, unsigned char *buffer);
int write_sector(unsigned int sector, const unsigned char *buffer);
unsigned short fat_next_cluster(unsigned short cluster);
unsigned short fat_get_entry(unsigned short cluster);
void fat_set_entry(unsigned short cluster, unsigned short value);

int name_match(Fat16Entry *entry, char *filename);
int find_in_directory(unsigned short dir_cluster, char *name, int want_directory, Fat16Entry *out_entry);
unsigned short get_parent_cluster(unsigned short dir_cluster);

void entry_name_copy(Fat16Entry *entry, char *name);
void entry_ext_copy(Fat16Entry *entry, char *ext);

#endif