#include <stdio.h>
#include "fat.h"
#include "fat_linux_adapter.h"

int main(void) {
    if (fat_linux_adapter_open("sd.img") != 0) {
        printf("ERR: open sd.img\n");
        return 1;
    }
    fat16_init();

    if (changeDir("ADR2") != 0) {
        printf("ERR: changeDir ADR2\n");
        return 2;
    }

    delete("NOVY.TXT");

    if (!freopen("in.txt", "rb", stdin)) {
        printf("ERR: cannot open input file in.txt\n");
        fat_linux_adapter_close();
        return 3;
    }

    if (write("NOVY.TXT") != 0) {
        printf("ERR: write NOVY.TXT\n");
        fat_linux_adapter_close();
        return 4;
    }

    printf("--- AFTER WRITE ---\n");
    dir_listing();
    putchar('\n');

    read_file("NOVY.TXT");
    putchar('\n');

    if (delete("NOVY.TXT") != 0) {
        printf("ERR: delete NOVY.TXT\n");
        fat_linux_adapter_close();
        return 5;
    }

    printf("--- AFTER DELETE ---\n");
    dir_listing();
    putchar('\n');

    fat_linux_adapter_close();
    printf("OK\n");
    return 0;
}