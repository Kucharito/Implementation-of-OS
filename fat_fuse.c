#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>

#include "fat_internal.h"
#include "fat_linux_adapter.h"

static void uppercase_ascii(char *s)
{
    for (unsigned int i = 0; s[i] != 0; i++)
    {
        if (s[i] >= 'a' && s[i] <= 'z')
            s[i] = (char)(s[i] - ('a' - 'A'));
    }
}

static int fat_resolve_path(const char *path, Fat16Entry *entry, int *is_dir)
{
    if (!path)
        return -ENOENT;

    if (str_eq(path, "/"))
    {
        if (is_dir)
            *is_dir = 1;
        return 0;
    }

    unsigned short current_cluster = 0;
    const char *cursor = path;
    char component[13];

    if (cursor[0] == '/')
        cursor++;

    while (parse_path_component(&cursor, component, sizeof(component)))
    {
        if (component[0] == 0 || str_eq(component, "."))
            continue;

        if (str_eq(component, ".."))
        {
            current_cluster = get_parent_cluster(current_cluster);
            continue;
        }

        uppercase_ascii(component);

        const char *peek = cursor;
        char next_component[13];
        int has_next = parse_path_component(&peek, next_component, sizeof(next_component));

        if (has_next)
        {
            Fat16Entry dir_entry;
            if (!find_in_directory(current_cluster, component, 1, &dir_entry))
                return -ENOENT;

            current_cluster = (dir_entry.starting_cluster < 2) ? 0 : dir_entry.starting_cluster;
            continue;
        }

        if (find_in_directory(current_cluster, component, 1, entry))
        {
            if (is_dir)
                *is_dir = 1;
            return 0;
        }

        if (find_in_directory(current_cluster, component, 0, entry))
        {
            if (is_dir)
                *is_dir = 0;
            return 0;
        }

        return -ENOENT;
    }

    return -ENOENT;
}

static void fat_entry_name(const Fat16Entry *entry, char *out, unsigned int out_len)
{
    char name[9];
    char ext[4];

    entry_name_copy((Fat16Entry *)entry, name);
    entry_ext_copy((Fat16Entry *)entry, ext);

    if ((entry->attributes & 0x10) || ext[0] == 0)
        snprintf(out, out_len, "%s", name);
    else
        snprintf(out, out_len, "%s.%s", name, ext);
}

static int fat_split_parent_path(const char *path,
                                 unsigned short *parent_cluster,
                                 char *name)
{
    unsigned short current_cluster = 0;
    const char *cursor = path;
    char component[13];

    if (!path || !parent_cluster || !name || str_eq(path, "/"))
        return -EINVAL;

    if (cursor[0] == '/')
        cursor++;

    while (parse_path_component(&cursor, component, sizeof(component)))
    {
        if (component[0] == 0 || str_eq(component, "."))
            continue;

        if (str_eq(component, ".."))
        {
            current_cluster = get_parent_cluster(current_cluster);
            continue;
        }

        uppercase_ascii(component);

        const char *peek = cursor;
        char next_component[13];
        int has_next = parse_path_component(&peek, next_component, sizeof(next_component));

        if (!has_next)
        {
            *parent_cluster = current_cluster;
            k_memcpy(name, component, sizeof(component));
            return 0;
        }

        Fat16Entry dir_entry;
        if (!find_in_directory(current_cluster, component, 1, &dir_entry))
            return -ENOENT;

        current_cluster = (dir_entry.starting_cluster < 2) ? 0 : dir_entry.starting_cluster;
    }

    return -EINVAL;
}

static int fat_read_entry_data(const Fat16Entry *entry,
                               char *buf,
                               unsigned int size,
                               unsigned int offset)
{
    if (!entry || !buf)
        return -EINVAL;

    if (offset >= entry->file_size)
        return 0;

    unsigned int to_read = size;
    if (offset + to_read > entry->file_size)
        to_read = entry->file_size - offset;

    if (to_read == 0 || entry->starting_cluster < 2)
        return 0;

    const unsigned int cluster_size = bs.sector_size * bs.sectors_per_cluster;
    unsigned int skip_clusters = offset / cluster_size;
    unsigned int in_cluster_offset = offset % cluster_size;
    unsigned short cluster = entry->starting_cluster;
    unsigned int copied = 0;
    unsigned char local_sector[SECTOR_SIZE];

    while (skip_clusters && cluster >= 2 && cluster < 0xFFF8)
    {
        cluster = fat_next_cluster(cluster);
        skip_clusters--;
    }

    if (cluster < 2 || cluster >= 0xFFF8)
        return 0;

    while (cluster >= 2 && cluster < 0xFFF8 && copied < to_read)
    {
        unsigned int first_sector = data_start_lba + (cluster - 2) * bs.sectors_per_cluster;
        unsigned int sector_index = in_cluster_offset / bs.sector_size;
        unsigned int sector_offset = in_cluster_offset % bs.sector_size;

        for (unsigned int s = sector_index; s < bs.sectors_per_cluster && copied < to_read; s++)
        {
            if (!read_sector(first_sector + s, local_sector))
                return -EIO;

            unsigned int from = (s == sector_index) ? sector_offset : 0;
            unsigned int available = bs.sector_size - from;
            unsigned int need = to_read - copied;
            unsigned int chunk = (available < need) ? available : need;

            k_memcpy(buf + copied, local_sector + from, chunk);
            copied += chunk;
        }

        in_cluster_offset = 0;
        cluster = fat_next_cluster(cluster);
    }

    return (int)copied;
}

static int fat_write_entry_data(unsigned short parent_cluster,
                                char *name,
                                const Fat16Entry *old_entry,
                                const char *data,
                                unsigned int size)
{
    DirSlot slot;
    Fat16Entry current_entry;
    unsigned short old_start = 0;
    unsigned short first_cluster = 0;
    unsigned short prev_cluster = 0;
    unsigned int remaining = size;
    unsigned int written = 0;
    unsigned char sector_data[SECTOR_SIZE];

    if (!find_dir_slot(parent_cluster, name, 0, &slot, &current_entry))
        return -ENOENT;

    old_start = current_entry.starting_cluster;

    while (remaining > 0)
    {
        unsigned short new_cluster = find_free_cluster();
        if (new_cluster == 0)
        {
            if (first_cluster >= 2)
                free_chain(first_cluster);
            return -ENOSPC;
        }

        fat_set_entry(new_cluster, 0xFFFF);

        if (prev_cluster >= 2)
            fat_set_entry(prev_cluster, new_cluster);
        else
            first_cluster = new_cluster;

        prev_cluster = new_cluster;

        unsigned int first_sector = data_start_lba + (new_cluster - 2) * bs.sectors_per_cluster;
        for (unsigned int s = 0; s < bs.sectors_per_cluster; s++)
        {
            unsigned int chunk = (remaining > bs.sector_size) ? bs.sector_size : remaining;

            k_memset(sector_data, 0, SECTOR_SIZE);
            if (chunk > 0)
            {
                k_memcpy(sector_data, data + written, chunk);
                written += chunk;
                remaining -= chunk;
            }

            if (!write_sector(first_sector + s, sector_data))
            {
                if (first_cluster >= 2)
                    free_chain(first_cluster);
                return -EIO;
            }

            if (remaining == 0)
                break;
        }
    }

    if (!read_sector(slot.sector, sector_buffer))
    {
        if (first_cluster >= 2)
            free_chain(first_cluster);
        return -EIO;
    }

    Fat16Entry *entry = (Fat16Entry *)(sector_buffer + slot.offset);
    k_memset(entry, 0, sizeof(Fat16Entry));
    format_83_name(name, entry->filename, entry->ext);
    entry->attributes = 0x20;
    entry->starting_cluster = first_cluster;
    entry->file_size = size;

    if (!write_sector(slot.sector, sector_buffer))
    {
        if (first_cluster >= 2)
            free_chain(first_cluster);
        return -EIO;
    }

    if (old_entry && old_start >= 2)
        free_chain(old_start);

    return 0;
}

static int fat_create_empty_file(const char *path)
{
    unsigned short parent_cluster = 0;
    char name[13];
    DirSlot slot;

    if (fat_split_parent_path(path, &parent_cluster, name) != 0)
        return -ENOENT;

    if (name[0] == 0)
        return -EINVAL;

    if (find_in_directory(parent_cluster, name, 0, NULL) ||
        find_in_directory(parent_cluster, name, 1, NULL))
        return -EEXIST;

    if (!find_dir_slot(parent_cluster, name, 1, &slot, NULL))
        return -ENOSPC;

    if (!read_sector(slot.sector, sector_buffer))
        return -EIO;

    Fat16Entry *entry = (Fat16Entry *)(sector_buffer + slot.offset);
    k_memset(entry, 0, sizeof(Fat16Entry));
    format_83_name(name, entry->filename, entry->ext);
    entry->attributes = 0x20;
    entry->starting_cluster = 0;
    entry->file_size = 0;

    if (!write_sector(slot.sector, sector_buffer))
        return -EIO;

    return 0;
}

static int fat_write_path(const char *path,
                          const char *buf,
                          size_t size,
                          off_t offset)
{
    unsigned short parent_cluster = 0;
    char name[13];
    Fat16Entry entry;
    char *new_data = NULL;
    unsigned int old_size;
    unsigned int new_size;

    if (offset < 0)
        return -EINVAL;

    if (fat_split_parent_path(path, &parent_cluster, name) != 0)
        return -ENOENT;

    if (!find_dir_slot(parent_cluster, name, 0, NULL, &entry))
        return -ENOENT;

    old_size = entry.file_size;
    new_size = (unsigned int)offset + (unsigned int)size;
    if (new_size < old_size)
        new_size = old_size;

    if (new_size > 0)
    {
        new_data = (char *)malloc(new_size);
        if (!new_data)
            return -ENOMEM;

        k_memset(new_data, 0, new_size);

        if (old_size > 0)
        {
            int read_res = fat_read_entry_data(&entry, new_data, old_size, 0);
            if (read_res < 0)
            {
                free(new_data);
                return read_res;
            }
        }

        if (size > 0)
            k_memcpy(new_data + (unsigned int)offset, buf, (unsigned int)size);
    }

    int write_res = fat_write_entry_data(parent_cluster, name, &entry, new_data, new_size);
    free(new_data);

    if (write_res != 0)
        return write_res;

    return (int)size;
}

static int fat_truncate_path(const char *path, off_t size)
{
    unsigned short parent_cluster = 0;
    char name[13];
    Fat16Entry entry;
    char *new_data = NULL;
    unsigned int new_size;

    if (size < 0)
        return -EINVAL;

    if (fat_split_parent_path(path, &parent_cluster, name) != 0)
        return -ENOENT;

    if (!find_dir_slot(parent_cluster, name, 0, NULL, &entry))
        return -ENOENT;

    new_size = (unsigned int)size;

    if (new_size > 0)
    {
        new_data = (char *)malloc(new_size);
        if (!new_data)
            return -ENOMEM;

        k_memset(new_data, 0, new_size);

        unsigned int keep = (entry.file_size < new_size) ? entry.file_size : new_size;
        if (keep > 0)
        {
            int read_res = fat_read_entry_data(&entry, new_data, keep, 0);
            if (read_res < 0)
            {
                free(new_data);
                return read_res;
            }
        }
    }

    int write_res = fat_write_entry_data(parent_cluster, name, &entry, new_data, new_size);
    free(new_data);
    return write_res;
}

static int fat_fill_dir(unsigned short dir_cluster,
                        void *buf,
                        fuse_fill_dir_t filler)
{
    int done = 0;

    if (dir_cluster == 0)
    {
        for (unsigned int i = 0; i < root_dir_sectors && !done; i++)
        {
            if (!read_sector(root_start_lba + i, sector_buffer))
                return -EIO;

            for (unsigned int j = 0; j < bs.sector_size / sizeof(Fat16Entry); j++)
            {
                Fat16Entry *entry = (Fat16Entry *)(sector_buffer + j * sizeof(Fat16Entry));

                if (entry->filename[0] == 0x00)
                {
                    done = 1;
                    break;
                }

                if (entry->filename[0] == 0xE5)
                    continue;

                if (entry->attributes & 0x08)
                    continue;

                if ((entry->attributes & 0x10) && entry->filename[0] == '.')
                    continue;

                char display[13];
                fat_entry_name(entry, display, sizeof(display));
                if (filler(buf, display, NULL, 0, 0) != 0)
                    return 0;
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
            if (!read_sector(first_sector + s, sector_buffer))
                return -EIO;

            for (unsigned int j = 0; j < bs.sector_size / sizeof(Fat16Entry); j++)
            {
                Fat16Entry *entry = (Fat16Entry *)(sector_buffer + j * sizeof(Fat16Entry));

                if (entry->filename[0] == 0x00)
                    return 0;

                if (entry->filename[0] == 0xE5)
                    continue;

                if (entry->attributes & 0x08)
                    continue;

                if ((entry->attributes & 0x10) && entry->filename[0] == '.')
                    continue;

                char display[13];
                fat_entry_name(entry, display, sizeof(display));
                if (filler(buf, display, NULL, 0, 0) != 0)
                    return 0;
            }
        }

        cluster = fat_next_cluster(cluster);
    }

    return 0;
}

static void *fat_init(struct fuse_conn_info *conn, struct fuse_config *cfg)
{
    (void) conn;
    cfg->kernel_cache = 1;
    fat16_init();
    return NULL;
}

static int fat_getattr(const char *path, struct stat *stbuf,
                       struct fuse_file_info *fi)
{
    (void) fi;
    k_memset(stbuf, 0, sizeof(struct stat));

    if (str_eq(path, "/"))
    {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }

    Fat16Entry entry;
    int is_dir = 0;

    if (fat_resolve_path(path, &entry, &is_dir) != 0)
        return -ENOENT;

    if (is_dir)
    {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
    }
    else
    {
        stbuf->st_mode = S_IFREG | 0644;
        stbuf->st_nlink = 1;
        stbuf->st_size = entry.file_size;
    }

    return 0;
}

static int fat_readdir(const char *path,
                       void *buf,
                       fuse_fill_dir_t filler,
                       off_t offset,
                       struct fuse_file_info *fi,
                       enum fuse_readdir_flags flags)
{
    (void) offset;
    (void) fi;
    (void) flags;

    Fat16Entry entry;
    int is_dir = 0;
    unsigned short dir_cluster = 0;

    if (str_eq(path, "/"))
    {
        dir_cluster = 0;
    }
    else
    {
        if (fat_resolve_path(path, &entry, &is_dir) != 0)
            return -ENOENT;

        if (!is_dir)
            return -ENOTDIR;

        dir_cluster = (entry.starting_cluster < 2) ? 0 : entry.starting_cluster;
    }

    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    return fat_fill_dir(dir_cluster, buf, filler);
}

static int fat_open(const char *path, struct fuse_file_info *fi)
{
    Fat16Entry entry;
    int is_dir = 0;

    if (fat_resolve_path(path, &entry, &is_dir) != 0)
        return -ENOENT;

    if (is_dir)
        return -EISDIR;

    (void) fi;

    return 0;
}

static int fat_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    (void) mode;
    (void) fi;
    return fat_create_empty_file(path);
}


static int fat_read(const char *path,
                    char *buf,
                    size_t size,
                    off_t offset,
                    struct fuse_file_info *fi)
{
    (void) fi;

    if (offset < 0)
        return -EINVAL;

    Fat16Entry entry;
    int is_dir = 0;

    if (fat_resolve_path(path, &entry, &is_dir) != 0)
        return -ENOENT;

    if (is_dir)
        return -EISDIR;

    return fat_read_entry_data(&entry, buf, (unsigned int)size, (unsigned int)offset);
}

static int fat_write(const char *path,
                     const char *buf,
                     size_t size,
                     off_t offset,
                     struct fuse_file_info *fi)
{
    (void) fi;
    return fat_write_path(path, buf, size, offset);
}

static int fat_truncate(const char *path, off_t size, struct fuse_file_info *fi)
{
    (void) fi;
    return fat_truncate_path(path, size);
}

static const struct fuse_operations fat_oper = {
    .init       = fat_init,
    .getattr    = fat_getattr,
    .readdir    = fat_readdir,
    .create     = fat_create,
    .open       = fat_open,
    .read       = fat_read,
    .write      = fat_write,
    .truncate   = fat_truncate,
};

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        fprintf(stderr, "Usage: %s [FUSE options] <sd.img> <mountpoint>\n", argv[0]);
        return 1;
    }

    const char *image_path = argv[argc - 2];
    if (image_path[0] == '-')
    {
        fprintf(stderr, "Expected image path before mountpoint, got option '%s'\n", image_path);
        return 1;
    }

    if (fat_linux_adapter_open(image_path) != 0)
    {
        perror("fat_linux_adapter_open");
        return 1;
    }

    int fuse_argc = argc - 1;
    char **fuse_argv = (char **)malloc(sizeof(char *) * (unsigned int)fuse_argc);
    if (!fuse_argv)
    {
        fat_linux_adapter_close();
        return 1;
    }

    int out_i = 0;
    for (int i = 0; i < argc; i++)
    {
        if (i == argc - 2)
            continue;

        if (str_eq(argv[i], "-o") && (i + 1) < argc)
        {
            if ((i + 1) == argc - 2)
            {
                fprintf(stderr, "Missing value for -o before image path\n");
                free(fuse_argv);
                fat_linux_adapter_close();
                return 1;
            }

            if (str_eq(argv[i + 1], "loop"))
            {
                i++;
                continue;
            }
        }

        fuse_argv[out_i++] = argv[i];
    }

    int ret = fuse_main(out_i, fuse_argv, &fat_oper, NULL);

    free(fuse_argv);
    fat_linux_adapter_close();
    return ret;
}

//fusermount3 -u -z mountpoint || umount -l mountpoint
//./fat_fuse -f -d -o loop sd.img mountpoint/
//fusermount3 -u mountpoint


//ls -la mountpoint && head -n 5 mountpoint/ABSTRAKT.TXT