#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>

#include "fat_internal.h"
#include "fat_linux_adapter.h"

static void uppercase_ascii(char *str)
{
    for(unsigned int i=0;str[i]!=0;i++)
    {
        if(str[i]>='a' && str[i]<='z')
            str[i] = (char)(str[i] - ('a' - 'A'));
    }
}

static int fall_fill_dir(unsigned short dir_cluster, void *buf, fuse_fill_dir_t filler)
{
    int done = 0;
    if(dir_cluster == 0)
    {
        for (unsigned int i=0;i<root_dir_sectors && !done;i++)
        {
            if(!read_sector(root_start_lba+i, sector_buffer))
                return -EIO;
            for(unsigned int j=0;j<bs.sector_size / sizeof(Fat16Entry);j++)
            {
                Fat16Entry * entry = (Fat16Entry *)(sector_buffer +j * sizeof(Fat16Entry));
                if(entry->filename[0] == 0x00)
                {
                    done = 1;
                    break;
                }
                if(entry->filename[0] == 0xE5)
                    continue;
                if(entry->attributes & 0x08)
                    continue;
                if((entry->attributes & 0x10) && entry->filename[0] == '.')
                    continue;
                char display[13];
                fat_entry_name(entry, display, sizeof(display));
                if(filler(buf, display, NULL, 0, 0) != 0)
                    return 0;
            }
        }
        return 0;
    }
    unsigned shor cluster = dir_cluster;
    while(cluster >= 2 && cluster < 0xFFF8)
    {
        unsigned int first_sector = data_start_lba + (cluster -2) * bs.sectors_per_cluster;
        for(unsigned int s=0;s<bs.sectors_per_cluster;s++)
        {
            if(!read_sector(first_sector + s, sector_buffer))
                return -EIO;
            for(unsigned int j=0;j<bs.sector_size / sizeof(Fat16Entry);j++)
            {
                Fat16Entry * entry = (Fat16Entry *)(sector_buffer + j * sizeof(Fat16Entry));
                if(entry->filename[0] == 0x00)
                    return 0;
                if(entry->filename[0] == 0xE5)
                    continue;
                if(entry->attributes & 0x08)
                    continue;
                if((entry->attributes & 0x10) && entry->filename[0] == '.')
                    continue;
                char display[13];
                fat_entry_name(entry, display, sizeof(display));
                if(filler(buf, display, NULL, 0, 0) != 0)
                    return 0;
            }
        }
        cluster = fat_next_cluster(cluster);
    }
    return 0;
}

static void *fat_init(struct fuse_conn_info *conn, struct fuse_config *cfg);    
{
    (void) conn;
    cfg -> kernel_cache = 1;
    fat16_init();
    return NULL;
}

static void fat_entry_name(const Fat16Entry *entry, char *out , unsigned int out_len)
{
    char name[9];
    char ext[4];
    entry_name_copy((Fat16Entry *)entry, name);
    entry_ext_copy((Fat16Entry *)entry, ext);
    if(entry->attributes & 0x10) || ext[0] == 0)
        snprintf(out, out_len, "%s", name);
    else
        snprintf(out, out_len, "%s.%s", name, ext);
}

static int fat_getattr(const char *path, struct stat *stbuf, strcut fuse_file_info *fi)
{
    (void) fi;
    memset(stbuf, 0, sizeof(struct stat));
    if(str_eq(path, "/"))
    {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }
    Fat16Entry entry;
    int is_dir = 0;
    if(fat_resolve_path(path,&entry, &is_dir)!= 0)
        return -ENOENT;
    if(is_dir)
    {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
    }
    else
    {
        stbuf->st_mode = S_IFREG | 0444;
        stbuf->st_nlink = 1;
        stbuf->st_size = entry.file_size;
    }
    return 0;
}