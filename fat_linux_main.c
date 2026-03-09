#include <stdio.h>

#include "fat.h"
#include "fat_linux_adapter.h"

int main(void)
{
    if (fat_linux_adapter_open("sd.img") != 0)
        return 1;

    fat16_init();

    printf("=== PRINT TREE ===\n");
    printTree();

    printf("\n=== changeDir(\"ADR2\") ===\n");
    if (changeDir("ADR2") == 0)
    {
        printf("=== DIR ADR2 ===\n");
        dir_listing();

        printf("\n=== read_file(\"KOREN.TXT\") in ADR2 ===\n");
        read_file("KOREN.TXT");
    }
    else
    {
        printf("changeDir failed\n");
    }

    fat_linux_adapter_close();
    return 0;
}