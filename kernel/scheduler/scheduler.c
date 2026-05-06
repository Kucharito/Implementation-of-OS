#include "scheduler.h"

struct gt gt_table[MAX_PROCESSES];
struct gt *gt_current = 0;

void scheduler_init(void)
{
	uint32_t i;

	for (i = 0; i < (uint32_t)MAX_PROCESSES; i++) {
		gt_table[i].esp = 0;
		gt_table[i].eip = 0;
		gt_table[i].base = PROC_BASE(i);
		gt_table[i].stack_top = PROC_STACK_TOP(i);
		gt_table[i].state = GT_UNUSED;
	}

	gt_current = 0;
}

void scheduler_tick(void)
{
	uint32_t i;
	uint32_t start;
	struct gt *next = 0;

	if (gt_current == 0) {
		start = 0;
	} else {
		start = (uint32_t)(gt_current - &gt_table[0]);
	}

	for (i = 1; i <= (uint32_t)MAX_PROCESSES; i++) {
		uint32_t idx = (start + i) % (uint32_t)MAX_PROCESSES;
		if (gt_table[idx].state == GT_READY) {
			next = &gt_table[idx];
			break;
		}
	}

	if (next == 0 || next == gt_current) {
		return;
	}

	if (gt_current && gt_current->state == GT_RUNNING) {
		gt_current->state = GT_READY;
	}

	next->state = GT_RUNNING;
	gt_current = next;
}

int scheduler_find_free_slot(uint32_t *out_slot)
{
	uint32_t i;

	if (out_slot == 0) {
		return -1;
	}

	for (i = 0; i < (uint32_t)MAX_PROCESSES; i++) {
		if (gt_table[i].state == GT_UNUSED) {
			*out_slot = i;
			return 0;
		}
	}

	return -1;
}

void scheduler_create_slot(uint32_t slot, uint32_t entry)
{
	uint32_t *sp;

	if (slot >= (uint32_t)MAX_PROCESSES) {
		return;
	}

	gt_table[slot].base = PROC_BASE(slot);
	gt_table[slot].stack_top = PROC_STACK_TOP(slot);

	sp = (uint32_t *)(gt_table[slot].stack_top + 4u);
	*(--sp) = 0x202u;                    // EFLAGS (IF=1)
	*(--sp) = 0x08u;                     // CS
	*(--sp) = entry;                     // EIP
	*(--sp) = 0;                         // EAX
	*(--sp) = 0;                         // ECX
	*(--sp) = 0;                         // EDX
	*(--sp) = 0;                         // EBX
	*(--sp) = gt_table[slot].stack_top;  // ESP (ignored by popa)
	*(--sp) = 0;                         // EBP
	*(--sp) = 0;                         // ESI
	*(--sp) = 0;                         // EDI

	gt_table[slot].esp = (uint32_t)sp;
	gt_table[slot].eip = entry;
	gt_table[slot].state = GT_READY;
}
