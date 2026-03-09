#include "fat_internal.h"

typedef struct {
    unsigned int sector;
    unsigned int offset;
} DirSlot;

static void format_83_name(const char *filename, unsigned char out_name[8], unsigned char out_ext[3])
{
    for (int i = 0; i < 8; i++)
        out_name[i] = ' ';
    for (int i = 0; i < 3; i++)
        out_ext[i] = ' ';

    int i = 0;
    while (filename[i] && filename[i] != '.' && i < 8)
    {
        char c = filename[i];
        if (c >= 'a' && c <= 'z')
            c = (char)(c - ('a' - 'A'));
        out_name[i] = (unsigned char)c;
        i++;
    }

    if (filename[i] == '.')
        i++;

    int j = 0;
    while (filename[i] && j < 3)
    {
        char c = filename[i];
        if (c >= 'a' && c <= 'z')
            c = (char)(c - ('a' - 'A'));
        out_ext[j++] = (unsigned char)c;
        i++;
    }
}

static int find_dir_slot(unsigned short dir_cluster, char *name, int want_free, DirSlot *slot, Fat16Entry *entry_out)
{
    int seen_end = 0;

    if (dir_cluster == 0)
    {
        for (unsigned int i = 0; i < root_dir_sectors; i++)
        {
            unsigned int sec = root_start_lba + i;
            read_sector(sec, sector_buffer);

            for (unsigned int j = 0; j < bs.sector_size / sizeof(Fat16Entry); j++)
            {
                Fat16Entry *entry = (Fat16Entry *)(sector_buffer + j * sizeof(Fat16Entry));
                unsigned int off = j * sizeof(Fat16Entry);

                if (want_free)
                {
                    if (entry->filename[0] == 0xE5 || entry->filename[0] == 0x00)
                    {
                        if (slot)
                        {
                            slot->sector = sec;
                            slot->offset = off;
                        }
                        return 1;
                    }
                    continue;
                }

                if (entry->filename[0] == 0x00)
                    return 0;
                if (entry->filename[0] == 0xE5)
                    continue;
                if (entry->attributes & 0x08)
                    continue;
                if (entry->attributes & 0x10)
                    continue;

                if (name_match(entry, name))
                {
                    if (slot)
                    {
                        slot->sector = sec;
                        slot->offset = off;
                    }
                    if (entry_out)
                        k_memcpy(entry_out, entry, sizeof(Fat16Entry));
                    return 1;
                }
            }
        }
        return 0;
    }

    unsigned short cluster = dir_cluster;
    while (cluster >= 2 && cluster < 0xFFF8)
    {
        unsigned int first_sector = data_start_lba + (cluster - 2) * bs.sectors_per_cluster;

        for (unsigned int s = 0; s < bs.sectors_per_cluster; s++)
        {
            unsigned int sec = first_sector + s;
            read_sector(sec, sector_buffer);

            for (unsigned int j = 0; j < bs.sector_size / sizeof(Fat16Entry); j++)
            {
                Fat16Entry *entry = (Fat16Entry *)(sector_buffer + j * sizeof(Fat16Entry));
                unsigned int off = j * sizeof(Fat16Entry);

                if (want_free)
                {
                    if (entry->filename[0] == 0xE5 || entry->filename[0] == 0x00)
                    {
                        if (slot)
                        {
                            slot->sector = sec;
                            slot->offset = off;
                        }
                        return 1;
                    }
                    continue;
                }

                if (entry->filename[0] == 0x00)
                {
                    seen_end = 1;
                    break;
                }
                if (entry->filename[0] == 0xE5)
                    continue;
                if (entry->attributes & 0x08)
                    continue;
                if (entry->attributes & 0x10)
                    continue;

                if (name_match(entry, name))
                {
                    if (slot)
                    {
                        slot->sector = sec;
                        slot->offset = off;
                    }
                    if (entry_out)
                        k_memcpy(entry_out, entry, sizeof(Fat16Entry));
                    return 1;
                }
            }

            if (seen_end)
                break;
        }

        if (seen_end)
            break;

        cluster = fat_next_cluster(cluster);
    }

    return 0;
}

static unsigned short find_free_cluster(void)
{
    unsigned int total_clusters = (bs.fat_size_sectors * bs.sector_size) / 2;
    for (unsigned short c = 2; c < total_clusters; c++)
    {
        if (fat_get_entry(c) == 0x0000)
            return c;
    }
    return 0;
}

static void free_chain(unsigned short start_cluster)
{
    unsigned short cluster = start_cluster;
    while (cluster >= 2 && cluster < 0xFFF8)
    {
        unsigned short next = fat_next_cluster(cluster);
        fat_set_entry(cluster, 0x0000);
        cluster = next;
    }
}

int write(char *filename)
{
    DirSlot slot;
    Fat16Entry existing;
    unsigned int file_size = 0;
    unsigned short first_cluster = 0;
    unsigned short prev_cluster = 0;
    unsigned short current_cluster = 0;
    unsigned int sector_in_cluster = 0;

    if (!filename || filename[0] == 0)
        return -1;

    if (find_dir_slot(cwd_cluster, filename, 0, 0, &existing))
        return -1;

    if (!find_dir_slot(cwd_cluster, filename, 1, &slot, 0))
        return -1;

    while (1)
    {
        unsigned char out_sector[SECTOR_SIZE];
        unsigned int read_bytes = fat_input_read(out_sector, SECTOR_SIZE);
        if (read_bytes == 0)
            break;

        if (current_cluster == 0 || sector_in_cluster >= bs.sectors_per_cluster)
        {
            unsigned short new_cluster = find_free_cluster();
            if (new_cluster == 0)
            {
                free_chain(first_cluster);
                return -1;
            }

            fat_set_entry(new_cluster, 0xFFFF);

            if (prev_cluster != 0)
                fat_set_entry(prev_cluster, new_cluster);
            else
                first_cluster = new_cluster;

            prev_cluster = new_cluster;
            current_cluster = new_cluster;
            sector_in_cluster = 0;
        }

        if (read_bytes < SECTOR_SIZE)
        {
            for (unsigned int i = read_bytes; i < SECTOR_SIZE; i++)
                out_sector[i] = 0;
        }

        unsigned int first_sector = data_start_lba + (current_cluster - 2) * bs.sectors_per_cluster;
        write_sector(first_sector + sector_in_cluster, out_sector);

        sector_in_cluster++;
        file_size += read_bytes;

        if (read_bytes < SECTOR_SIZE)
            break;
    }

    read_sector(slot.sector, sector_buffer);
    Fat16Entry *entry = (Fat16Entry *)(sector_buffer + slot.offset);

    for (unsigned int i = 0; i < sizeof(Fat16Entry); i++)
        ((unsigned char *)entry)[i] = 0;

    format_83_name(filename, entry->filename, entry->ext);
    entry->attributes = 0x20;
    entry->starting_cluster = first_cluster;
    entry->file_size = file_size;

    write_sector(slot.sector, sector_buffer);
    return 0;
}

int delete(char *filename)
{
    DirSlot slot;
    Fat16Entry entry;

    if (!filename || filename[0] == 0)
        return -1;

    if (!find_dir_slot(cwd_cluster, filename, 0, &slot, &entry))
        return -1;

    if (entry.starting_cluster >= 2)
        free_chain(entry.starting_cluster);

    read_sector(slot.sector, sector_buffer);
    ((Fat16Entry *)(sector_buffer + slot.offset))->filename[0] = 0xE5;
    write_sector(slot.sector, sector_buffer);

    return 0;
}