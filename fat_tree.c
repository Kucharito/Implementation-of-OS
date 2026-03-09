#include "fat_internal.h"

static int is_dot_or_dotdot(Fat16Entry *entry)
{
    return (entry->filename[0] == '.');
}

static void print_indent(unsigned int depth)
{
    for (unsigned int i = 0; i < depth; i++)
        print_string("  ");
}

static void print_tree_entry(Fat16Entry *entry, unsigned int depth)
{
    char name[9];
    char ext[4];

    entry_name_copy(entry, name);

    print_indent(depth);
    print_string(name);

    if (entry->attributes & 0x10)
    {
        print_string("/\n");
        return;
    }

    entry_ext_copy(entry, ext);
    if (ext[0])
    {
        console_putc('.');
        print_string(ext);
    }
    console_putc('\n');
}

static void print_tree_dir(unsigned short dir_cluster, unsigned int depth)
{
    int done = 0;

    if (dir_cluster == 0)
    {
        for (unsigned int i = 0; i < root_dir_sectors && !done; i++)
        {
            read_sector(root_start_lba + i, sector_buffer);

            for (unsigned int j = 0;
                 j < bs.sector_size / sizeof(Fat16Entry);
                 j++)
            {
                Fat16Entry *entry =
                    (Fat16Entry *)(sector_buffer + j * sizeof(Fat16Entry));

                if (entry->filename[0] == 0x00)
                {
                    done = 1;
                    break;
                }

                if (entry->filename[0] == 0xE5)
                    continue;

                if (entry->attributes & 0x08)
                    continue;

                if ((entry->attributes & 0x10) && is_dot_or_dotdot(entry))
                    continue;

                print_tree_entry(entry, depth);

                if ((entry->attributes & 0x10) && entry->starting_cluster >= 2)
                {
                    unsigned char saved_sector[SECTOR_SIZE];
                    k_memcpy(saved_sector, sector_buffer, SECTOR_SIZE);
                    print_tree_dir(entry->starting_cluster, depth + 1);
                    k_memcpy(sector_buffer, saved_sector, SECTOR_SIZE);
                }
            }

            if (done)
                break;
        }

        return;
    }

    unsigned short scan_cluster = dir_cluster;
    while (scan_cluster >= 2 && scan_cluster < 0xFFF8 && !done)
    {
        unsigned int first_sector =
            data_start_lba + (scan_cluster - 2) * bs.sectors_per_cluster;

        for (unsigned int i = 0; i < bs.sectors_per_cluster; i++)
        {
            read_sector(first_sector + i, sector_buffer);

            for (unsigned int j = 0;
                 j < bs.sector_size / sizeof(Fat16Entry);
                 j++)
            {
                Fat16Entry *entry =
                    (Fat16Entry *)(sector_buffer + j * sizeof(Fat16Entry));

                if (entry->filename[0] == 0x00)
                {
                    done = 1;
                    break;
                }

                if (entry->filename[0] == 0xE5)
                    continue;

                if (entry->attributes & 0x08)
                    continue;

                if ((entry->attributes & 0x10) && is_dot_or_dotdot(entry))
                    continue;

                print_tree_entry(entry, depth);

                if ((entry->attributes & 0x10) && entry->starting_cluster >= 2)
                {
                    unsigned char saved_sector[SECTOR_SIZE];
                    k_memcpy(saved_sector, sector_buffer, SECTOR_SIZE);
                    print_tree_dir(entry->starting_cluster, depth + 1);
                    k_memcpy(sector_buffer, saved_sector, SECTOR_SIZE);
                }
            }

            if (done)
                break;
        }

        if (!done)
            scan_cluster = fat_next_cluster(scan_cluster);
    }
}

void printTree(void)
{
    print_string("/\n");
    print_tree_dir(0, 1);
}