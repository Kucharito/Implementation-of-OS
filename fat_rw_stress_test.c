#include <stdio.h>

#include "fat.h"
#include "fat_linux_adapter.h"

static int set_input(const char *path)
{
    return freopen(path, "rb", stdin) ? 0 : -1;
}

int main(int argc, char **argv)
{
    const char *image = (argc > 1) ? argv[1] : "sd.img";

    if (fat_linux_adapter_open(image) != 0)
    {
        printf("ERR: open image\n");
        return 1;
    }

    fat16_init();
    if (changeDir("ADR2") != 0)
    {
        printf("ERR: changeDir ADR2\n");
        fat_linux_adapter_close();
        return 2;
    }

    delete("A.TXT");
    delete("B.TXT");
    delete("C.TXT");
    delete("D.TXT");

    if (set_input("/tmp/fat_in_a.txt") != 0 || write("A.TXT") != 0)
    {
        printf("ERR: write A.TXT\n");
        fat_linux_adapter_close();
        return 3;
    }

    if (set_input("/tmp/fat_in_b.txt") != 0 || write("B.TXT") != 0)
    {
        printf("ERR: write B.TXT\n");
        fat_linux_adapter_close();
        return 4;
    }

    if (delete("A.TXT") != 0)
    {
        printf("ERR: delete A.TXT\n");
        fat_linux_adapter_close();
        return 5;
    }

    if (set_input("/tmp/fat_in_c.txt") != 0 || write("C.TXT") != 0)
    {
        printf("ERR: write C.TXT\n");
        fat_linux_adapter_close();
        return 6;
    }

    if (delete("B.TXT") != 0)
    {
        printf("ERR: delete B.TXT\n");
        fat_linux_adapter_close();
        return 7;
    }

    if (set_input("/tmp/fat_in_d.txt") != 0 || write("D.TXT") != 0)
    {
        printf("ERR: write D.TXT\n");
        fat_linux_adapter_close();
        return 8;
    }

    printf("--- FINAL ADR2 LISTING ---\n");
    dir_listing();
    printf("--- READ C.TXT ---\n");
    read_file("C.TXT");
    printf("\n--- READ D.TXT ---\n");
    read_file("D.TXT");
    printf("\nSTRESS_OK\n");

    fat_linux_adapter_close();
    return 0;
}
