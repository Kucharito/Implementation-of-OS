#include "fat_internal.h"

int changeDir(char *path)
{
    char component[13];
    const char *p = path;
    unsigned short target_cluster;

    if (!path || path[0] == 0)
        return 0;

    target_cluster = (path[0] == '/') ? 0 : cwd_cluster;

    while (parse_path_component(&p, component, sizeof(component)))
    {
        if (component[0] == 0)
            continue;

        if (str_eq(component, "."))
            continue;

        if (str_eq(component, ".."))
        {
            target_cluster = get_parent_cluster(target_cluster);
            continue;
        }

        Fat16Entry next_dir;
        if (!find_in_directory(target_cluster, component, 1, &next_dir))
            return -1;

        if (next_dir.starting_cluster < 2)
            target_cluster = 0;
        else
            target_cluster = next_dir.starting_cluster;
    }

    cwd_cluster = target_cluster;
    return 0;
}

void dir_listing()
{
    unsigned int file_count = 0;
    unsigned int dir_count = 0;
    unsigned int total_size = 0;
    int done = 0;

    unsigned short scan_cluster = cwd_cluster;

    while (!done)
    {
        unsigned int first_sector;
        unsigned int sectors_to_scan;

        if (scan_cluster == 0)
        {
            first_sector = root_start_lba;
            sectors_to_scan = root_dir_sectors;
            done = 1;
        }
        else
        {
            first_sector = data_start_lba + (scan_cluster - 2) * bs.sectors_per_cluster;
            sectors_to_scan = bs.sectors_per_cluster;
        }

        for (unsigned int i = 0; i < sectors_to_scan; i++)
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

                unsigned short year = ((entry->modify_date >> 9) & 0x7F) + 1980;
                unsigned short month = (entry->modify_date >> 5) & 0x0F;
                unsigned short day = entry->modify_date & 0x1F;

                unsigned short hour = (entry->modify_time >> 11) & 0x1F;
                unsigned short min = (entry->modify_time >> 5) & 0x3F;

                char name[9];
                char ext[4];

                entry_name_copy(entry, name);
                entry_ext_copy(entry, ext);

                print_2d(month);
                console_putc('/');
                print_2d(day);
                console_putc('/');
                print_dec(year);
                console_putc(' ');

                print_2d(hour);
                console_putc(':');
                print_2d(min);
                print_string("      ");

                if (entry->attributes & 0x10)
                {
                    print_string("   <DIR>              ");
                    print_string(name);
                    console_putc('\n');
                    dir_count++;
                }
                else
                {
                    print_dec_width(entry->file_size, 16);
                    print_string("      ");
                    print_string(name);
                    if (ext[0])
                    {
                        console_putc('.');
                        print_string(ext);
                    }
                    console_putc('\n');

                    file_count++;
                    total_size += entry->file_size;
                }
            }

            if (done)
                break;
        }

        if (scan_cluster != 0 && !done)
            scan_cluster = fat_next_cluster(scan_cluster);

        if (scan_cluster >= 0xFFF8)
            done = 1;
    }

    console_putc('\n');
    print_dec(file_count);
    print_string(" File(s) ");
    print_dec(total_size);
    print_string(" bytes\n");

    print_dec(dir_count);
    print_string(" Dir(s)\n");
}