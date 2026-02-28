#include "fat.h"


static u8 sector_buffer[SECTOR_SIZE];
static PartitionTable pt[4];
static Fat16BootSector bs;


int ata_read_sector(u32 lba, u8 *buffer);
void console_putc(char c);
void console_write(const char *buf, u32 len);


void *k_memcpy(void *dest, const void *src, u32 n)
{
    u8 *d = dest;
    const u8 *s = src;

    for (u32 i = 0; i < n; i++)
        d[i] = s[i];

    return dest;
}

u32 k_strlen(const char *str)
{
    u32 len = 0;
    while (str[len])
        len++;
    return len;
}


void print_string(const char *s)
{
    console_write(s, k_strlen(s));
}

void print_dec(u32 value)
{
    char buffer[16];
    int i = 0;

    if (value == 0)
    {
        console_putc('0');
        return;
    }

    while (value > 0)
    {
        buffer[i++] = '0' + (value % 10);
        value /= 10;
    }

    while (i--)
        console_putc(buffer[i]);
}



int read_sector(u32 sector, u8 *buffer)
{
    return ata_read_sector(sector, buffer);
}



void fat16_init()
{
    read_sector(0, sector_buffer);
    k_memcpy(pt, sector_buffer + 0x1BE, sizeof(pt));

    read_sector(pt[0].start_sector, sector_buffer);
    k_memcpy(&bs, sector_buffer, sizeof(Fat16BootSector));
}



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


void read_file(char *filename)
{
    u32 partition_start = pt[0].start_sector;

    u32 fat_start =
        partition_start + bs.reserved_sectors;

    u32 root_start =
        partition_start +
        bs.reserved_sectors +
        bs.number_of_fats * bs.fat_size_sectors;

    u32 root_dir_sectors =
        (bs.root_dir_entries * sizeof(Fat16Entry) + bs.sector_size - 1) / bs.sector_size;

    u32 data_start =
        root_start + root_dir_sectors;

    u16 start_cluster = 0;
    u32 file_size = 0;
    int found = 0;
    for (u32 i = 0; i < root_dir_sectors; i++)
    {

        read_sector(root_start + i, sector_buffer);

        for (u32 j = 0;
             j < bs.sector_size / sizeof(Fat16Entry);
             j++)
        {

            Fat16Entry *entry =
                (Fat16Entry *)(sector_buffer + j * sizeof(Fat16Entry));

            if (entry->filename[0] == 0x00)
                break;

            if (entry->filename[0] == 0xE5)
                continue;

            if (entry->attributes & 0x10)
                continue;
            
            if (name_match(entry, filename))
            {
                start_cluster = entry->starting_cluster;
                file_size = entry->file_size;
                
                found = 1;
                break;
            }
        }
    }

    if (!start_cluster)
        return;

    u16 cluster = start_cluster;
    u32 remaining = file_size;
    while (cluster < 0xFFF8)
    {

        u32 first_sector =
            data_start + (cluster - 2) * bs.sectors_per_cluster;

        for (u32 s = 0;
             s < bs.sectors_per_cluster && remaining;
             s++)
        {

            read_sector(first_sector + s, sector_buffer);

            u32 to_write =
                remaining < bs.sector_size
                    ? remaining
                    : bs.sector_size;

            console_write((char *)sector_buffer,
                          to_write);

            remaining -= to_write;

            if (!remaining)
                return;
        }

        u32 fat_sector =
            fat_start + (cluster * 2) / bs.sector_size;

        u32 fat_offset =
            (cluster * 2) % bs.sector_size;

        read_sector(fat_sector, sector_buffer);

        u8 *p = sector_buffer + fat_offset;
        cluster = p[0] | (p[1] << 8);
    }
}

void dir_listing()
{
    u32 partition_start = pt[0].start_sector;

    u32 root_start =
        partition_start +
        bs.reserved_sectors +
        bs.number_of_fats * bs.fat_size_sectors;

    u32 root_dir_sectors =
        (bs.root_dir_entries * sizeof(Fat16Entry) + bs.sector_size - 1) / bs.sector_size;

    u32 file_count = 0;
    u32 dir_count = 0;
    u32 total_size = 0;

    for (u32 i = 0; i < root_dir_sectors; i++)
    {

        read_sector(root_start + i, sector_buffer);

        for (u32 j = 0;
             j < bs.sector_size / sizeof(Fat16Entry);
             j++)
        {

            Fat16Entry *entry =
                (Fat16Entry *)(sector_buffer + j * sizeof(Fat16Entry));

            if (entry->filename[0] == 0x00)
                return;

            if (entry->filename[0] == 0xE5)
                continue;

            if (entry->attributes & 0x08)
                continue;

            /* dátum */
            u16 year = ((entry->modify_date >> 9) & 0x7F) + 1980;
            u16 month = (entry->modify_date >> 5) & 0x0F;
            u16 day = entry->modify_date & 0x1F;

            /* čas */
            u16 hour = (entry->modify_time >> 11) & 0x1F;
            u16 min = (entry->modify_time >> 5) & 0x3F;

            char name[9];
            char ext[4];

            k_memcpy(name, entry->filename, 8);
            k_memcpy(ext, entry->ext, 3);

            name[8] = 0;
            ext[3] = 0;

            for (int k = 7; k >= 0 && name[k] == ' '; k--)
                name[k] = 0;

            for (int k = 2; k >= 0 && ext[k] == ' '; k--)
                ext[k] = 0;

            /* výpis dátumu */
            print_dec(month);
            console_putc('/');
            print_dec(day);
            console_putc('/');
            print_dec(year);
            console_putc(' ');

            print_dec(hour);
            console_putc(':');
            print_dec(min);
            console_putc(' ');
            console_putc(' ');

            if (entry->attributes & 0x10)
            {

                print_string("<DIR>      ");
                print_string(name);
                console_putc('\n');
                dir_count++;
            }
            else
            {

                print_dec(entry->file_size);
                print_string("  ");
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
    }

    console_putc('\n');
    print_dec(file_count);
    print_string(" File(s) ");
    print_dec(total_size);
    print_string(" bytes\n");

    print_dec(dir_count);
    print_string(" Dir(s)\n");
}