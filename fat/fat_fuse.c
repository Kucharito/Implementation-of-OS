#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>

#include "fat_internal.h"
#include "fat_linux_adapter.h"

/* Normalizuje cast cesty na uppercase, aby sedela s FAT 8.3 zaznamami. */
static void uppercase_ascii(char *s)
{
    for (unsigned int i = 0; s[i] != 0; i++)
    {
        if (s[i] >= 'a' && s[i] <= 'z')
            s[i] = (char)(s[i] - ('a' - 'A'));
    }
}

/* Prelozi absolutnu cestu na FAT polozku a vrati, ci je to dir/file. */
static int fat_resolve_path(const char *path, Fat16Entry *entry, int *is_dir)
{
    // Ak je cesta NULL, vrátime chybu -ENOENT (neexistuje entita).
    if (!path)
        return -ENOENT;

    // Ak je cesta len "/", znamená to koreňový adresár.
    if (str_eq(path, "/"))
    {
        if (is_dir)
            *is_dir = 1;  // Nastavíme indikátor, že je to adresár.
        return 0; 
    }

    unsigned short current_cluster = 0;  
    const char *cursor = path;  
    char component[13]; 

    if (cursor[0] == '/')
        cursor++;

    // Prechádzame cez jednotlivé časti cesty (napr. "folder1", "folder2").
    while (parse_path_component(&cursor, component, sizeof(component)))
    {
        if (component[0] == 0 || str_eq(component, "."))
            continue;

        if (str_eq(component, ".."))
        {
            current_cluster = get_parent_cluster(current_cluster);  // Vrátime sa na rodičovský "cluster".
            continue;
        }

        uppercase_ascii(component);

        // Snažíme sa zistiť, či existuje ďalšia časť cesty.
        const char *peek = cursor;
        char next_component[13];
        int has_next = parse_path_component(&peek, next_component, sizeof(next_component));

        // Ak existuje ďalšia časť, znamená to, že ide o adresár.
        if (has_next)
        {
            Fat16Entry dir_entry;
            // Snažíme sa nájsť adresár.
            if (!find_in_directory(current_cluster, component, 1, &dir_entry))
                return -ENOENT; 

            current_cluster = (dir_entry.starting_cluster < 2) ? 0 : dir_entry.starting_cluster;  // Aktualizujeme cluster.
            continue;
        }

        // Ak je to súbor, pokúsime sa ho nájsť.
        if (find_in_directory(current_cluster, component, 1, entry))
        {
            if (is_dir)
                *is_dir = 1;  // Ak je to adresár, nastavíme indikátor.
            return 0;  
        }

        // Ak to nie je adresár, skontrolujeme, či je to obyčajný súbor.
        if (find_in_directory(current_cluster, component, 0, entry))
        {
            if (is_dir)
                *is_dir = 0;  // Ak je to súbor, nastavíme indikátor na 0.
            return 0; 
        }

        return -ENOENT;
    }

    return -ENOENT;
}

/* Sformatuje meno FAT polozky na citatelny retazec NAME alebo NAME.EXT. */
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

/* Prejde jeden adresar a posle viditelne mena do FUSE filler callbacku. */
static int fat_fill_dir(unsigned short dir_cluster,void *buf,fuse_fill_dir_t filler)
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

/* FUSE init hook: zapne cache a inicializuje FAT16 metadata. */
static void *fat_init(struct fuse_conn_info *conn, struct fuse_config *cfg)
{
    (void)conn;
    cfg->kernel_cache = 1;
    fat16_init();
    return NULL;
}

/* FUSE getattr hook: namapuje cestu na stat metadata suboru alebo adresara. */
static int fat_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi)
{
    (void)fi; 

    // Nastavíme štruktúru 'stat' na nulu.
    k_memset(stbuf, 0, sizeof(struct stat));

    // Ak je cesta "/" (koreňový adresár).
    if (str_eq(path, "/"))
    {
        // S_IFDIR označuje, že ide o adresár, a 0755 sú prístupové práva.
        stbuf->st_mode = S_IFDIR | 0755;
        // Nastavíme počet odkazov na adresár.
        stbuf->st_nlink = 2;
        return 0; 
    }

    Fat16Entry entry; 
    int is_dir = 0;  

    // Pokúsime sa vyriešiť cestu a získať informácie o zodpovedajúcom súbore/adresári.
    if (fat_resolve_path(path, &entry, &is_dir) != 0)
        return -ENOENT; 

    // Ak ide o adresár, nastavíme jeho atribúty.
    if (is_dir)
    {
        stbuf->st_mode = S_IFDIR | 0755;  // Adresár s právami 0755.
        stbuf->st_nlink = 2;  // Počet odkazov na adresár.
    }
    else  // Ak ide o súbor.
    {
        stbuf->st_mode = S_IFREG | 0444;  // Súbor s prístupovými právami 0444 (iba na čítanie).
        stbuf->st_nlink = 1;  // Počet odkazov na súbor.
        stbuf->st_size = entry.file_size;  // Veľkosť súboru.
    }

    return 0;  // Úspešne nastavené atribúty pre súbor alebo adresár.
}

/* FUSE readdir hook: emit '.', '..' and directory children names. */
static int fat_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags)
{
    (void)offset;  
    (void)fi;      
    (void)flags;   

    Fat16Entry entry;  
    int is_dir = 0;    
    unsigned short dir_cluster = 0; 

    // Ak je cesta "/" (koreňový adresár).
    if (str_eq(path, "/"))
    {
        dir_cluster = 0;  // Koreňový adresár začína od clusteru 0.
    }
    else  // Inak sa snažíme nájsť konkrétny adresár podľa cesty.
    {
        // Pokúsime sa vyriešiť cestu a získať informácie o adresári.
        if (fat_resolve_path(path, &entry, &is_dir) != 0)
            return -ENOENT;  

        if (!is_dir) 
            return -ENOTDIR;

        dir_cluster = (entry.starting_cluster < 2) ? 0 : entry.starting_cluster;  // Nastavíme správny cluster.
    }

    // Vypisujeme "." (aktuálny adresár) a ".." (rodičovský adresár).
    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    // Naplníme buffer zoznamom súborov/adresárov v danom adresári.
    return fat_fill_dir(dir_cluster, buf, filler);
}
/* FUSE open hook: povoli len existujuce regular subory v read-only mode. */
static int fat_open(const char *path, struct fuse_file_info *fi)
{
    Fat16Entry entry;
    int is_dir = 0;

    if (fat_resolve_path(path, &entry, &is_dir) != 0)
        return -ENOENT;

    if (is_dir)
        return -EISDIR;

    if ((fi->flags & O_ACCMODE) != O_RDONLY)
        return -EACCES;

    return 0;
}

/* FUSE read hook: skopiruje pozadovany rozsah bajtov z FAT retazca do buffera. */
static int fat_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    (void)fi;  

    if (offset < 0)
        return -EINVAL;

    Fat16Entry entry;  
    int is_dir = 0;   

    // Pokúsime sa vyriešiť cestu a získať informácie o súbore.
    if (fat_resolve_path(path, &entry, &is_dir) != 0)
        return -ENOENT; 

    // Ak je to adresár, vrátime chybu -EISDIR (je to adresár, nie súbor).
    if (is_dir)
        return -EISDIR;

    // Ak offset presahuje veľkosť súboru, znamená to, že sa pokúšame čítať mimo súboru.
    if ((unsigned int)offset >= entry.file_size)
        return 0;  

    unsigned int to_read = (unsigned int)size;  // Nastavíme, koľko dát chceme čítať.
    // Ak offset + požiadavka na čítanie presahuje veľkosť súboru, upravíme to_read na zvyšnú veľkosť.
    if ((unsigned int)offset + to_read > entry.file_size)
        to_read = entry.file_size - (unsigned int)offset;

    // Ak nie je čo čítať, alebo začiatok súboru nie je v platnom clusteri, vrátime 0.
    if (to_read == 0 || entry.starting_cluster < 2)
        return 0;

    const unsigned int cluster_size = bs.sector_size * bs.sectors_per_cluster;  // Veľkosť jedného clusteru v bajtoch.
    unsigned int skip_clusters = (unsigned int)offset / cluster_size;  // Počet clusterov, ktoré treba preskočiť na začiatok požadovaného offsetu.
    unsigned int in_cluster_offset = (unsigned int)offset % cluster_size;  // Ofset v rámci aktuálneho clusteru.
    unsigned short cluster = entry.starting_cluster;  // Začínajúci cluster súboru.

    // Preskakujeme požadovaný počet clusterov, aby sme sa dostali na správny začiatok.
    while (skip_clusters && cluster >= 2 && cluster < 0xFFF8)
    {
        cluster = fat_next_cluster(cluster);  // Prechádzame na ďalší cluster.
        skip_clusters--;  // Znižujeme počet zostávajúcich clusterov na preskočenie.
    }

    // Ak sme dosiahli neplatný cluster, vrátime 0.
    if (cluster < 2 || cluster >= 0xFFF8)
        return 0;

    unsigned int copied = 0;  // Počet skopírovaných bajtov.
    unsigned char local_sector[SECTOR_SIZE];  // Buffer pre načítanie jedného sektoru.

    // Prechádzame cez clustery, až kým neprečítame všetky požadované dáta.
    while (cluster >= 2 && cluster < 0xFFF8 && copied < to_read)
    {
        unsigned int first_sector = data_start_lba + (cluster - 2) * bs.sectors_per_cluster;  // Prvý sektor v aktuálnom clusteri.
        unsigned int sector_index = in_cluster_offset / bs.sector_size;  // Index sektora v rámci clusteru.
        unsigned int sector_offset = in_cluster_offset % bs.sector_size;  // Ofset v rámci sektora.

        // Pre každý sektor v rámci clusteru.
        for (unsigned int s = sector_index; s < bs.sectors_per_cluster && copied < to_read; s++)
        {
            if (!read_sector(first_sector + s, local_sector))  // Načítame sektor do lokálneho bufferu.
                return -EIO;  

            unsigned int from = (s == sector_index) ? sector_offset : 0;  // Počiatočný ofset v sektore.
            unsigned int available = bs.sector_size - from;  // Zostávajúci počet bajtov v sektore.
            unsigned int need = to_read - copied;  // Počet bajtov, ktoré ešte treba skopírovať.
            unsigned int chunk = (available < need) ? available : need;  // Určíme, koľko bajtov naozaj skopírovať.

            k_memcpy(buf + copied, local_sector + from, chunk);  // Skopírujeme dáta do bufferu.
            copied += chunk;  // Aktualizujeme počet skopírovaných bajtov.
        }

        in_cluster_offset = 0;  // Po prečítaní sektora, začíname od začiatku clusteru.
        cluster = fat_next_cluster(cluster);  // Prejdeme na ďalší cluster.
    }

    return (int)copied;  // Vrátime počet skopírovaných bajtov (môže byť menej než požiadavka, ak sa dosiahol koniec súboru).
}
/* Tabulka mapovania FUSE operacii. */
static const struct fuse_operations fat_oper = {
    .init = fat_init,
    .getattr = fat_getattr,
    .readdir = fat_readdir,
    .open = fat_open,
    .read = fat_read,
};

/* Spracuje CLI argumenty, otvori image a spusti FUSE event loop. */
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
//make fat_fuse
//./fat_fuse -f -d -o loop sd.img mountpoint/
// ls -la mountpoint &&head -n 5 mountpoint/ABSTRAKT.TXT

// odpojenie //fusermount3 -u mountpoint