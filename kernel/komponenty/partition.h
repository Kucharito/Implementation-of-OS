#ifndef PARTITION_H
#define PARTITION_H

#include <stdint.h>

#define KERNEL_BASE 0x00001000u
#define PROC_BASE_START 0x00100000u
#define PROC_SLOT_SIZE 0x00010000u
#define MAX_PROCESSES 8u

#define PROC_BASE(i) (PROC_BASE_START + ((uint32_t)(i) * PROC_SLOT_SIZE))
#define PROC_STACK_TOP(i) (PROC_BASE(i) + PROC_SLOT_SIZE - 4u)

struct partition {
    uint32_t base;
    int used;
};

extern struct partition partition_table[MAX_PROCESSES];

void partition_init(void);

#endif
