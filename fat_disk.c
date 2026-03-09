#include "fat_internal.h"

int read_sector(unsigned int sector, unsigned char *buffer)
{
    return ata_read_sector(sector, buffer);
}

int write_sector(unsigned int sector, const unsigned char *buffer)
{
    return ata_write_sector(sector, buffer);
}

unsigned short fat_get_entry(unsigned short cluster)
{
    unsigned int fat_sector =
        fat_start_lba + (cluster * 2) / bs.sector_size;

    unsigned int fat_offset =
        (cluster * 2) % bs.sector_size;

    read_sector(fat_sector, sector_buffer);

    return (unsigned short)(sector_buffer[fat_offset] |
                            (sector_buffer[fat_offset + 1] << 8));
}

void fat_set_entry(unsigned short cluster, unsigned short value)
{
    unsigned int fat_sector_rel = (cluster * 2) / bs.sector_size;
    unsigned int fat_offset = (cluster * 2) % bs.sector_size;

    for (unsigned int copy = 0; copy < bs.number_of_fats; copy++)
    {
        unsigned int fat_sector = fat_start_lba + copy * bs.fat_size_sectors + fat_sector_rel;
        read_sector(fat_sector, sector_buffer);
        sector_buffer[fat_offset] = (unsigned char)(value & 0xFF);
        sector_buffer[fat_offset + 1] = (unsigned char)((value >> 8) & 0xFF);
        write_sector(fat_sector, sector_buffer);
    }
}

unsigned short fat_next_cluster(unsigned short cluster)
{
    return fat_get_entry(cluster);
}