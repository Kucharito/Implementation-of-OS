#include <stdint.h>
#include "pic.h"
#include "timer.h"
#include "scheduler.h"

uint32_t isr_handler(uint32_t *stack)
{
    if (gt_current) {
        gt_current->esp = (uint32_t)(uintptr_t)stack;
        gt_current->eip = stack[8];
    }

    timer_callback();
    scheduler_tick();
    pic_send_eoi(0);

    if (gt_current && gt_current->esp != 0) {
        return gt_current->esp;
    }

    return (uint32_t)(uintptr_t)stack;
}
