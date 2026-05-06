#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>

#include "partition.h"

enum gt_state {
    GT_UNUSED = 0,
    GT_READY,
    GT_RUNNING
};

struct gt {
    uint32_t esp;
    uint32_t eip;
    uint32_t base;
    uint32_t stack_top;
    enum gt_state state;
};

extern struct gt gt_table[MAX_PROCESSES];
extern struct gt *gt_current;

void scheduler_init(void);
void scheduler_tick(void);
int scheduler_find_free_slot(uint32_t *out_slot);
void scheduler_create_slot(uint32_t slot, uint32_t entry);

#endif
