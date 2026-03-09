#include "fat_internal.h"

int name_match(Fat16Entry *entry, char *filename)
{
    int i = 0;

    for (i = 0; i < 8; i++)
    {
        if (filename[i] == '.' || filename[i] == 0)
            break;
        if (filename[i] != entry->filename[i])
            return 0;
    }

    while (filename[i] && filename[i] != '.')
        i++;

    if (filename[i] == '.')
        i++;

    for (int j = 0; j < 3; j++)
    {
        if (filename[i + j] == 0)
            break;
        if (filename[i + j] != entry->ext[j])
            return 0;
    }

    return 1;
}

int find_in_directory(unsigned short dir_cluster, char *name, int want_directory, Fat16Entry *out_entry)
{
    if (dir_cluster == 0)
    {
        for (unsigned int i = 0; i < root_dir_sectors; i++)
        {
            read_sector(root_start_lba + i, sector_buffer);
            for (unsigned int j = 0; j < bs.sector_size / sizeof(Fat16Entry); j++)
            {
                Fat16Entry *entry = (Fat16Entry *)(sector_buffer + j * sizeof(Fat16Entry));
                if (entry->filename[0] == 0x00)
                    return 0;
                if (entry->filename[0] == 0xE5)
                    continue;
                if (entry->attributes & 0x08)
                    continue;
                if (want_directory && !(entry->attributes & 0x10))
                    continue;
                if (!want_directory && (entry->attributes & 0x10))
                    continue;
                if (name_match(entry, name))
                {
                    if (out_entry)
                        k_memcpy(out_entry, entry, sizeof(Fat16Entry));
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
            read_sector(first_sector + s, sector_buffer);
            for (unsigned int j = 0; j < bs.sector_size / sizeof(Fat16Entry); j++)
            {
                Fat16Entry *entry = (Fat16Entry *)(sector_buffer + j * sizeof(Fat16Entry));
                if (entry->filename[0] == 0x00)
                    return 0;
                if (entry->filename[0] == 0xE5)
                    continue;
                if (entry->attributes & 0x08)
                    continue;
                if (want_directory && !(entry->attributes & 0x10))
                    continue;
                if (!want_directory && (entry->attributes & 0x10))
                    continue;
                if (name_match(entry, name))
                {
                    if (out_entry)
                        k_memcpy(out_entry, entry, sizeof(Fat16Entry));
                    return 1;
                }
            }
        }
        cluster = fat_next_cluster(cluster);
    }
    return 0;
}

void entry_name_copy(Fat16Entry *entry, char *name)
{
    k_memcpy(name, entry->filename, 8);
    name[8] = 0;

    for (int k = 7; k >= 0 && name[k] == ' '; k--)
        name[k] = 0;
}

void entry_ext_copy(Fat16Entry *entry, char *ext)
{
    k_memcpy(ext, entry->ext, 3);
    ext[3] = 0;

    for (int k = 2; k >= 0 && ext[k] == ' '; k--)
        ext[k] = 0;
}

unsigned short get_parent_cluster(unsigned short dir_cluster)
{
    Fat16Entry parent;
    if (dir_cluster == 0)
        return 0;

    if (!find_in_directory(dir_cluster, "..", 1, &parent))
        return 0;

    return parent.starting_cluster;
}