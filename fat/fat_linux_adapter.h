#ifndef FAT_LINUX_ADAPTER_H
#define FAT_LINUX_ADAPTER_H

/* Otvorenie/zatvorenie host disk image pouzivaneho ATA shim vrstvou. */
int fat_linux_adapter_open(const char *path);
void fat_linux_adapter_close(void);

#endif