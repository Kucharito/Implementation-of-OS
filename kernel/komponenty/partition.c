#include "partition.h"

struct partition partition_table[MAX_PROCESSES];

void partition_init(void)
{
    uint32_t i;

    for (i = 0; i < (uint32_t)MAX_PROCESSES; i++) {
        partition_table[i].base = PROC_BASE(i);
        partition_table[i].used = 0;
    }
}
