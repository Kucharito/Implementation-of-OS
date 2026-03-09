#ifndef FAT_API_H
#define FAT_API_H

void fat16_init(void);
int changeDir(char *path);
void printTree(void);
void read_file(char *filename);
int write(char *filename);
int delete(char *filename);
void dir_listing(void);

int ata_read_sector(unsigned int lba, unsigned char *buffer);
int ata_write_sector(unsigned int lba, const unsigned char *buffer);
unsigned int fat_input_read(unsigned char *buffer, unsigned int max_len);
void console_putc(char c);
void console_write(const char *buf, unsigned int len);

#endif