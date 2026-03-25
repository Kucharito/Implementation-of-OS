#include "fat_internal.h"

/* Nacita metadata particie a boot sektora a predpocita klucove FAT16 LBA oblasti. */
void fat16_init()
{
    read_sector(0, sector_buffer);
    k_memcpy(pt, sector_buffer + 0x1BE, sizeof(pt));

    read_sector(pt[0].start_sector, sector_buffer);
    k_memcpy(&bs, sector_buffer, sizeof(Fat16BootSector));

    fat_start_lba = pt[0].start_sector + bs.reserved_sectors;
    root_start_lba = fat_start_lba + bs.number_of_fats * bs.fat_size_sectors;
    root_dir_sectors =(bs.root_dir_entries * sizeof(Fat16Entry) + bs.sector_size - 1) / bs.sector_size;
    data_start_lba = root_start_lba + root_dir_sectors;
    cwd_cluster = 0;
}
//bez tejto funkcie kod nevie kde na disku lezi FAT root adresar ani data, a teda nevie citat ani zapisovat subory, ani navigovat adresarovu strukturu.