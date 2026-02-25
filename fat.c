#include "fat.h"
#include <stdio.h>
#include <stdlib.h>





int main() {
  FILE* in = fopen("sd.img", "rb");
  int i;
  PartitionTable pt[4];
  Fat16BootSector bs;
  Fat16Entry entry;

  fseek(in, 0x1BE, SEEK_SET);
  fread(pt, sizeof(PartitionTable), 4, in);

  printf("Partition table\n-----------------------\n");
  for (i = 0; i < 4; i++) {
    printf("Partition %d, type %02X, ", i, pt[i].partition_type);
    printf("start sector %8d, length %8d sectors\n",
           pt[i].start_sector, pt[i].length_sectors);
  }

  printf("\nSeeking to first partition by %d sectors\n",
         pt[0].start_sector);

  fseek(in, 512 * pt[0].start_sector, SEEK_SET);
  fread(&bs, sizeof(Fat16BootSector), 1, in);

  printf("Volume_label %.11s, %d sectors size\n",
         bs.volume_label, bs.sector_size);

  fseek(in,
        (bs.reserved_sectors - 1 +
         bs.fat_size_sectors * bs.number_of_fats)
        * bs.sector_size,
        SEEK_CUR);

  printf("\n Directory of FAT16\n\n");

  int file_count = 0;
  int dir_count = 0;
  unsigned int total_size = 0;

  for (i = 0; i < bs.root_dir_entries; i++) {

    fread(&entry, sizeof(entry), 1, in);

    if (entry.filename[0] == 0x00)
      break;

    if (entry.filename[0] == 0xE5)
      continue;

    if (entry.attributes & 0x08)
      continue;

    int year  = ((entry.modify_date >> 9) & 0x7F) + 1980;
    int month = (entry.modify_date >> 5) & 0x0F;
    int day   = entry.modify_date & 0x1F;

    int hour24 = (entry.modify_time >> 11) & 0x1F;
    int minute = (entry.modify_time >> 5) & 0x3F;

    char *ampm = "AM";
    int hour12 = hour24;

    if (hour24 >= 12) {
      ampm = "PM";
      if (hour24 > 12)
        hour12 = hour24 - 12;
    }
    if (hour12 == 0)
      hour12 = 12;

    char name[9];
    char ext[4];

    memcpy(name, entry.filename, 8);
    memcpy(ext, entry.ext, 3);

    name[8] = 0;
    ext[3] = 0;

    for (int j = 7; j >= 0 && name[j] == ' '; j--)
      name[j] = 0;

    for (int j = 2; j >= 0 && ext[j] == ' '; j--)
      ext[j] = 0;

    printf("%02d/%02d/%04d  %02d:%02d %s  ",
           month, day, year, hour12, minute, ampm);

    if (entry.attributes & 0x10) {
      printf("   <DIR>          %s\n", name);
      dir_count++;
    } else {
      printf("%14u   %s", entry.file_size, name);
      if (ext[0] != 0)
        printf(".%s", ext);
      printf("\n");

      file_count++;
      total_size += entry.file_size;
    }
  }

  printf("\n%14d File(s) %u bytes\n", file_count, total_size);
  printf("%14d Dir(s)\n", dir_count);

  fclose(in);
  return 0;
}