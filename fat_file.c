#include "fat_internal.h"

void read_file(char *filename)
{
    unsigned short start_cluster = 0;
    unsigned int file_size = 0;
    Fat16Entry file_entry;

    if (find_in_directory(cwd_cluster, filename, 0, &file_entry))
    {
        start_cluster = file_entry.starting_cluster;
        file_size = file_entry.file_size;
    }

    if (!start_cluster)
        return;

    unsigned short cluster = start_cluster;
    unsigned int remaining = file_size;
    while (cluster < 0xFFF8)
    {
        unsigned int first_sector =
            data_start_lba + (cluster - 2) * bs.sectors_per_cluster;

        for (unsigned int s = 0;
             s < bs.sectors_per_cluster && remaining;
             s++)
        {
            read_sector(first_sector + s, sector_buffer);

            unsigned int to_write =
                remaining < bs.sector_size
                    ? remaining
                    : bs.sector_size;

            console_write((char *)sector_buffer,
                          to_write);

            remaining -= to_write;

            if (!remaining)
                return;
        }

        cluster = fat_next_cluster(cluster);
    }
}