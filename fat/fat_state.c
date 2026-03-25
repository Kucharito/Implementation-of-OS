#include "fat_internal.h"

/* Zdielany jednosektorovy scratch buffer pre low-level skenery. */
unsigned char sector_buffer[SECTOR_SIZE];
/* Cache partition tabulky nacitana z MBR sektora 0. */
PartitionTable pt[4];
/* Cache FAT16 boot sektora s geometriou a metadatami filesystemu. */
Fat16BootSector bs;
/* Cluster aktualneho adresara (0 znamena FAT16 root adresar). */
unsigned short cwd_cluster;

/* LBA, kde zacina prva kopia FAT. */
unsigned int fat_start_lba;
/* LBA, kde zacina oblast FAT16 root adresara s pevnou velkostou. */
unsigned int root_start_lba;
/* Pocet sektorov obsadenych root adresarovou oblastou. */
unsigned int root_dir_sectors;
/* LBA, kde zacinaju datove clustre (cluster #2). */
unsigned int data_start_lba;