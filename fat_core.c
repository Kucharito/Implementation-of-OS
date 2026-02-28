#include "fat.h"

static unsigned char sector_buffer[SECTOR_SIZE];
static PartitionTable pt[4];
static Fat16BootSector bs;

int ata_read_sector(unsigned int lba, unsigned char *buffer);
void console_putc(char c);
void console_write(const char *buf, unsigned int len);

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

void print_string(const char *s)
{
    console_write(s, k_strlen(s));
}

void print_dec(unsigned int value)
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

int read_sector(unsigned int sector, unsigned char *buffer)
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
    unsigned int partition_start = pt[0].start_sector;

    unsigned int fat_start =
        partition_start + bs.reserved_sectors;

    unsigned int root_start =
        partition_start +
        bs.reserved_sectors +
        bs.number_of_fats * bs.fat_size_sectors;

    unsigned int root_dir_sectors =
        (bs.root_dir_entries * sizeof(Fat16Entry) + bs.sector_size - 1) / bs.sector_size;

    unsigned int data_start =
        root_start + root_dir_sectors;

    unsigned short start_cluster = 0;
    unsigned int file_size = 0;
    int found = 0;
    for (unsigned int i = 0; i < root_dir_sectors; i++)
    {

        read_sector(root_start + i, sector_buffer);

        for (unsigned int j = 0;
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

    unsigned short cluster = start_cluster;
    unsigned int remaining = file_size;
    while (cluster < 0xFFF8)
    {

        unsigned int first_sector =
            data_start + (cluster - 2) * bs.sectors_per_cluster;

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

        unsigned int fat_sector =
            fat_start + (cluster * 2) / bs.sector_size;

        unsigned int fat_offset =
            (cluster * 2) % bs.sector_size;

        read_sector(fat_sector, sector_buffer);

        unsigned char *p = sector_buffer + fat_offset;
        cluster = p[0] | (p[1] << 8);
    }
}

static void print_2d(unsigned int v)
{
    if (v < 10)
        console_putc('0');
    print_dec(v);
}
static unsigned int digits10(unsigned int v)
{
    unsigned int d = 1;
    while (v >= 10)
    {
        v /= 10;
        d++;
    }
    return d;
}

static void print_dec_width(unsigned int v, unsigned int w)
{
    unsigned int d = digits10(v);
    while (d < w) { console_putc(' '); d++; }
    print_dec(v);
}

void dir_listing()
{
    unsigned int partition_start = pt[0].start_sector;

    unsigned int root_start =
        partition_start +
        bs.reserved_sectors +
        bs.number_of_fats * bs.fat_size_sectors;

    unsigned int root_dir_sectors =
        (bs.root_dir_entries * sizeof(Fat16Entry) + bs.sector_size - 1) / bs.sector_size;

    unsigned int file_count = 0;
    unsigned int dir_count = 0;
    unsigned int total_size = 0;

    for (unsigned int i = 0; i < root_dir_sectors; i++)
    {

        read_sector(root_start + i, sector_buffer);

        for (unsigned int j = 0;
             j < bs.sector_size / sizeof(Fat16Entry);
             j++)
        {

            Fat16Entry *entry =
                (Fat16Entry *)(sector_buffer + j * sizeof(Fat16Entry));

            if (entry->filename[0] == 0x00)
                goto end_listing;

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

            k_memcpy(name, entry->filename, 8);
            k_memcpy(ext, entry->ext, 3);

            name[8] = 0;
            ext[3] = 0;

            for (int k = 7; k >= 0 && name[k] == ' '; k--)
                name[k] = 0;

            for (int k = 2; k >= 0 && ext[k] == ' '; k--)
                ext[k] = 0;

            /* dátum čas */
            print_2d(month);
            console_putc('/');
            print_2d(day);
            console_putc('/');
            print_dec(year);
            console_putc(' ');

            print_2d(hour);
            console_putc(':');
            print_2d(min);
            console_putc(' ');
            console_putc(' ');

            /* DOS stĺpce */
            if (entry->attributes & 0x10)
            {
                print_string("   <DIR>    ");
                print_string(name);
                console_putc('\n');
                dir_count++;
            }
            else
            {
                print_dec_width(entry->file_size, 10); // 10 znakov doprava
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

end_listing:

    console_putc('\n');
    print_dec(file_count);
    print_string(" File(s) ");
    print_dec(total_size);
    print_string(" bytes\n");

    print_dec(dir_count);
    print_string(" Dir(s)\n");
}