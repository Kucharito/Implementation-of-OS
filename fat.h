
typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;

#define SECTOR_SIZE 512

typedef struct {
    u8  first_byte;
    u8  start_chs[3];
    u8  partition_type;
    u8  end_chs[3];
    u32 start_sector;
    u32 length_sectors;
} __attribute__((packed)) PartitionTable;

typedef struct {
    u8  jmp[3];
    char oem[8];
    u16 sector_size;
    u8  sectors_per_cluster;
    u16 reserved_sectors;
    u8  number_of_fats;
    u16 root_dir_entries;
    u16 total_sectors_short;
    u8  media_descriptor;
    u16 fat_size_sectors;
    u16 sectors_per_track;
    u16 number_of_heads;
    u32 hidden_sectors;
    u32 total_sectors_int;
    u8  drive_number;
    u8  current_head;
    u8  boot_signature;
    u32 volume_id;
    char volume_label[11];
    char fs_type[8];
    u8  boot_code[448];
    u16 boot_sector_signature;
} __attribute__((packed)) Fat16BootSector;

typedef struct {
    u8  filename[8];
    u8  ext[3];
    u8  attributes;
    u8  reserved[10];
    u16 modify_time;
    u16 modify_date;
    u16 starting_cluster;
    u32 file_size;
} __attribute__((packed)) Fat16Entry;

/* abstrakcie */
void console_putc(char c);
void console_write(const char *buf, u32 len);
int disk_read_sector(u32 lba, u8 *buffer);
void *k_memcpy(void *dest, const void *src, u32 n);
void *k_memset(void *dest, int val, u32 n);
u32 k_strlen(const char *str);
