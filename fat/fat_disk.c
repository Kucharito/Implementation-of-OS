#include "fat_internal.h"

/* Precita jeden logicky sektor cez platformovy adapter. */
int read_sector(unsigned int sector, unsigned char *buffer)
{
    return ata_read_sector(sector, buffer);
}

/* Zapise jeden logicky sektor cez platformovy adapter. */
int write_sector(unsigned int sector, const unsigned char *buffer)
{
    return ata_write_sector(sector, buffer);
}

/* Precita hodnotu FAT16 polozky pre dany cluster. */
unsigned short fat_get_entry(unsigned short cluster)
{
    unsigned int fat_sector =fat_start_lba + (cluster * 2) / bs.sector_size;
    unsigned int fat_offset =(cluster * 2) % bs.sector_size;

    read_sector(fat_sector, sector_buffer);

    return (unsigned short)(sector_buffer[fat_offset] |(sector_buffer[fat_offset + 1] << 8));
}

/* Aktualizuje FAT16 polozku vo vsetkych FAT kopiach pre konzistenciu. */
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

/* Vrati dalsi cluster v retazci (alebo FAT marker). */
unsigned short fat_next_cluster(unsigned short cluster)
{
    return fat_get_entry(cluster);
}