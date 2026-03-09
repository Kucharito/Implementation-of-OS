#include "fat_internal.h"

unsigned char sector_buffer[SECTOR_SIZE];
PartitionTable pt[4];
Fat16BootSector bs;
unsigned short cwd_cluster;

unsigned int fat_start_lba;
unsigned int root_start_lba;
unsigned int root_dir_sectors;
unsigned int data_start_lba;