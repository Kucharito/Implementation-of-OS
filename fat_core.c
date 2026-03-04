#include "fat.h"

static unsigned char sector_buffer[SECTOR_SIZE];
static PartitionTable pt[4];
static Fat16BootSector bs;
static unsigned short cwd_cluster;

static unsigned int fat_start_lba;
static unsigned int root_start_lba;
static unsigned int root_dir_sectors;
static unsigned int data_start_lba;

int ata_read_sector(unsigned int lba, unsigned char *buffer);
void console_putc(char c);
void console_write(const char *buf, unsigned int len);
int name_match(Fat16Entry *entry, char *filename);

static int str_eq(const char *a, const char *b)
{
    while(*a && *b)
    {
        if(*a != *b)
            return 0;
        a++;
        b++;
    }
    return (*a == 0 && *b == 0);
}

static int parse_path_component(const char **path, char * component, unsigned int max_len)
{
    unsigned int i = 0;
    while(**path == '/')
        (*path)++;
    if(**path==0)
    {
        component[0] = 0;
        return 0;
    }
    while(**path && **path != '/')
    {
        if(i + 1 < max_len)
            component[i++]=**path;
        (*path)++;
    }
    component[i] = 0;
    return 1;
}
// Skopíruje n bajtov z src do dest.
void *k_memcpy(void *dest, const void *src, unsigned int n)
{
    unsigned char *d = dest;
    const unsigned char *s = src;

    for (unsigned int i = 0; i < n; i++)
        d[i] = s[i];

    return dest;
}

// Vráti dĺžku C reťazca bez ukončovacieho nulového znaku.
unsigned int k_strlen(const char *str)
{
    unsigned int len = 0;
    while (str[len])
        len++;
    return len;
}

// Vypíše celý reťazec na konzolu.
void print_string(const char *s)
{
    console_write(s, k_strlen(s));
}

// Vypíše nezáporné číslo v desiatkovej sústave.
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

// Tenký wrapper nad ATA čítaním jedného sektora.
int read_sector(unsigned int sector, unsigned char *buffer)
{
    return ata_read_sector(sector, buffer);
}

static unsigned short fat_next_cluster(unsigned short cluster)
{
    unsigned int fat_sector =
        fat_start_lba + (cluster * 2) / bs.sector_size;

    unsigned int fat_offset =
        (cluster * 2) % bs.sector_size;

    read_sector(fat_sector, sector_buffer);

    return (unsigned short)(sector_buffer[fat_offset] |
                            (sector_buffer[fat_offset + 1] << 8));
}

static int find_in_directory(unsigned short dir_cluster, char *name, int want_directory, Fat16Entry *out_entry)
{
    if(dir_cluster == 0)
    {
        for (unsigned int i = 0; i < root_dir_sectors; i++)
        {
            read_sector(root_start_lba + i, sector_buffer);
            for (unsigned int j=0; j <bs.sector_size / sizeof(Fat16Entry); j++)
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
                    if(out_entry)
                        k_memcpy(out_entry, entry, sizeof(Fat16Entry));
                    return 1;
                }
            }
        }
        return 0;
    }
    unsigned short cluster = dir_cluster;
    while(cluster >= 2 && cluster < 0xFFF8)
    {
        unsigned int first_sector = data_start_lba + (cluster -2 )* bs.sectors_per_cluster;
        for(unsigned int s = 0; s < bs.sectors_per_cluster; s++)
        {
            read_sector(first_sector + s, sector_buffer);
            for (unsigned int j=0; j <bs.sector_size / sizeof(Fat16Entry); j++)
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
                    if(out_entry)
                        k_memcpy(out_entry, entry, sizeof(Fat16Entry));
                    return 1;
                }
            }
        }
        cluster = fat_next_cluster(cluster);
    }
    return 0;

}
    


static unsigned short get_parent_cluster(unsigned short dir_cluster)
{
    Fat16Entry parent;
    if(dir_cluster == 0)
    {
        return 0;
    }
    if(!find_in_directory(dir_cluster, "..", 1, &parent))
    {
        return 0;
    }
    return parent.starting_cluster;
}

// Načíta MBR a FAT16 boot sektor z prvej partície.
void fat16_init()
{
    read_sector(0, sector_buffer);
    k_memcpy(pt, sector_buffer + 0x1BE, sizeof(pt));

    read_sector(pt[0].start_sector, sector_buffer);
    k_memcpy(&bs, sector_buffer, sizeof(Fat16BootSector));

    fat_start_lba = pt[0].start_sector + bs.reserved_sectors;
    root_start_lba = fat_start_lba + bs.number_of_fats * bs.fat_size_sectors;
    root_dir_sectors =
        (bs.root_dir_entries * sizeof(Fat16Entry) + bs.sector_size - 1) / bs.sector_size;
    data_start_lba = root_start_lba + root_dir_sectors;
    cwd_cluster = 0;
}

// Porovná 8.3 názov položky s požadovaným názvom súboru.
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

int changeDir(char *path)
{
    char component[13];
    const char *p = path;
    unsigned short target_cluster;

    if(!path || path[0] == 0)
        return 0;
    
    target_cluster = (path[0] == '/') ? 0 : cwd_cluster;

    while(parse_path_component(&p, component, sizeof(component)))
    {
        if(component[0]==0)
            continue;
        
        if(str_eq(component, "."))
        {
            continue;
        }
        if(str_eq(component, ".."))
        {
            target_cluster = get_parent_cluster(target_cluster);
            continue;
        }
        Fat16Entry next_dir;
        if(!find_in_directory(target_cluster, component,1, &next_dir))
        {
            return -1;
        }
        if(next_dir.starting_cluster<2)
            target_cluster = 0;
        else
            target_cluster = next_dir.starting_cluster;
        
    }
    cwd_cluster = target_cluster;
    return 0;
}




// Nájde súbor v root adresári a vypíše jeho obsah na konzolu.
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

// Vypíše číslo minimálne na 2 znaky (s nulou na začiatku).
static void print_2d(unsigned int v)
{
    if (v < 10)
        console_putc('0');
    print_dec(v);
}

// Zistí počet desiatkových číslic hodnoty.
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

// Vypíše číslo zarovnané doprava na danú šírku.
static void print_dec_width(unsigned int v, unsigned int w)
{
    unsigned int d = digits10(v);
    while (d < w)
    {
        console_putc(' ');
        d++;
    }
    print_dec(v);
}

// Vypíše obsah root adresára podobne ako jednoduchý DIR výpis.
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

                k_memcpy(name, entry->filename, 8);
                k_memcpy(ext, entry->ext, 3);

                name[8] = 0;
                ext[3] = 0;

                for (int k = 7; k >= 0 && name[k] == ' '; k--)
                    name[k] = 0;

                for (int k = 2; k >= 0 && ext[k] == ' '; k--)
                    ext[k] = 0;

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