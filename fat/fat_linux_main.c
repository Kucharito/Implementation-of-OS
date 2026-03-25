#include <stdio.h>

#include "fat.h"
#include "fat_linux_adapter.h"

/* Demo CLI vstupny bod pre operacie tree/list/read nad sd.img. */
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
    /*if (changeDir("/") != 0)
    {
        printf("changeDir to root failed\n");
    }

    if (delete("NOVY.TXT") != 0)
        printf("delete NOVY.TXT failed\n");

    if (delete("NOVY2.TXT") != 0)
        printf("delete NOVY2.TXT failed\n");*/

    fat_linux_adapter_close();
    return 0;
}