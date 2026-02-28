#include "fat.h"

/* ====================== */
/*  LOW LEVEL BUFFER      */
/* ====================== */

static unsigned char sector_buffer[SECTOR_SIZE];
static PartitionTable pt[4];
static Fat16BootSector bs;

/* ====================== */
/*  EXTERNAL INTERFACE    */
/* ====================== */

/* tieto funkcie implementuje adaptér */
int ata_read_sector(unsigned int lba, unsigned char *buffer);
void console_putc(char c);
void console_write(const char *buf, unsigned int len);

/* ====================== */
/*  MINIMAL LIBC          */
/* ====================== */

void *k_memcpy(void *dest, const void *src, unsigned int n)
{
    unsigned char *d = dest;
    const unsigned char *s = src;

    for (unsigned int i = 0; i < n; i++)
        d[i] = s[i];

    return dest;
}

unsigned int k_strlen(const char *str)
{
    unsigned int len = 0;
    while (str[len])
        len++;
    return len;
}

/* ====================== */
/*  SECTOR WRAPPER        */
/* ====================== */

int read_sector(unsigned int sector, unsigned char *buffer)
{
    return ata_read_sector(sector, buffer);
}

/* ====================== */
/*  INIT                  */
/* ====================== */

void fat16_init()
{
    read_sector(0, sector_buffer);
    k_memcpy(pt, sector_buffer + 0x1BE, sizeof(pt));

    read_sector(pt[0].start_sector, sector_buffer);
    k_memcpy(&bs, sector_buffer, sizeof(Fat16BootSector));
}

/* ====================== */
/*  NAME MATCH            */
/* ====================== */

int name_match(Fat16Entry *entry, char *filename)
{
    int i = 0;

    for (i = 0; i < 8; i++) {
        if (filename[i] == '.' || filename[i] == 0)
            break;
        if (filename[i] != entry->filename[i])
            return 0;
    }

    while (filename[i] && filename[i] != '.')
        i++;

    if (filename[i] == '.')
        i++;

    for (int j = 0; j < 3; j++) {
        if (filename[i + j] == 0)
            break;
        if (filename[i + j] != entry->ext[j])
            return 0;
    }

    return 1;
}

/* ====================== */
/*  FILE READER           */
/* ====================== */

void read_file(char *filename)
{
    unsigned int partition_start = pt[0].start_sector;

    unsigned int fat_start =
        partition_start + bs.reserved_sectors;

    unsigned int root_start =
        partition_start +
        bs.reserved_sectors +
        bs.number_of_fats * bs.fat_size_sectors;

    unsigned int root_dir_sectors =
        (bs.root_dir_entries * sizeof(Fat16Entry)
         + bs.sector_size - 1)
        / bs.sector_size;

    unsigned int data_start =
        root_start + root_dir_sectors;

    unsigned short start_cluster = 0;
    unsigned int file_size = 0;

    for (unsigned int i = 0; i < root_dir_sectors; i++) {

        read_sector(root_start + i, sector_buffer);

        for (unsigned int j = 0;
             j < bs.sector_size / sizeof(Fat16Entry);
             j++) {

            Fat16Entry *entry =
                (Fat16Entry*)
                (sector_buffer + j * sizeof(Fat16Entry));

            if (entry->filename[0] == 0x00)
                return;

            if (entry->filename[0] == 0xE5)
                continue;

            if (entry->attributes & 0x10)
                continue;

            if (name_match(entry, filename)) {
                start_cluster = entry->starting_cluster;
                file_size = entry->file_size;
                break;
            }
        }
    }

    if (!start_cluster)
        return;

    unsigned short cluster = start_cluster;
    unsigned int remaining = file_size;

    while (cluster < 0xFFF8) {

        unsigned int first_sector =
            data_start + (cluster - 2)
            * bs.sectors_per_cluster;

        for (unsigned int s = 0;
             s < bs.sectors_per_cluster && remaining;
             s++) {

            read_sector(first_sector + s, sector_buffer);

            unsigned int to_write =
                remaining < bs.sector_size
                ? remaining
                : bs.sector_size;

            console_write((char*)sector_buffer,
                          to_write);

            remaining -= to_write;

            if (!remaining)
                return;
        }

        unsigned int fat_sector =
            fat_start + (cluster * 2)
            / bs.sector_size;

        unsigned int fat_offset =
            (cluster * 2)
            % bs.sector_size;

        read_sector(fat_sector, sector_buffer);

        cluster =
            *(unsigned short*)
            (sector_buffer + fat_offset);
    }
}