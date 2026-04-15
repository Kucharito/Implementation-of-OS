#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "gthr.h"
#include "gthr_platform.h"
#include "gthr_struct.h"

static uint8_t gt_stacks[MaxGThreads][StackSize];
static bool gt_in_schedule;
static enum gt_sched_policy gt_sched_policy_current = GtSchedPRI;
static uint64_t gt_rng_state;
static int gt_effective_priority(const struct gt *t);

static int gt_clamp_priority(int priority){
	if (priority < GtPriorityHighest)
		return GtPriorityHighest;
	if (priority > GtPriorityLowest)
		return GtPriorityLowest;
	return priority;
}
static struct gt *gt_next_slot(struct gt *p){
	++p;
	if (p == &gt_table[MaxGThreads])
		return &gt_table[0];
	return p;
}

static uint32_t gt_rand_u32(void) {
	uint64_t x = gt_rng_state;

	if (x == 0)
		x = 0x9e3779b97f4a7c15ULL;

	x ^= x >> 12;
	x ^= x << 25;
	x ^= x >> 27;
	gt_rng_state = x;

	return (uint32_t)((x * 2685821657736338717ULL) >> 32);
}

static unsigned int gt_thread_ticket_count(const struct gt *t) {
	int base_priority = gt_clamp_priority(t->priority);

	/* 0 priority gets most tickets, 10 gets the fewest, but never zero. */
	return (unsigned int)(GtPriorityLowest - base_priority + 1);
}

static struct gt *gt_find_next_ready_rr(void) {
	struct gt *start = gt_next_slot(gt_current);
	struct gt *p = start;

	do {
		if (p->state == Ready)
			return p;
		p = gt_next_slot(p);
	} while (p != start);

	return NULL;
}

static struct gt *gt_find_next_ready_by_priority(void){
	int priority;
	struct gt *start = gt_next_slot(gt_current);
	
	for (priority = GtPriorityHighest; priority <= GtPriorityLowest; ++priority) {
		struct gt *p = start;

		do {
			if (p->state == Ready && gt_effective_priority(p) == priority)
				return p;
			p = gt_next_slot(p);
		} while (p != start);
	}

	return NULL;
}

static struct gt *gt_find_next_ready_lottery(void) {
	unsigned int total_tickets = 0;
	unsigned int draw;
	unsigned int cumulative = 0;
	int i;

	for (i = 0; i < MaxGThreads; ++i) {
		const struct gt *t = &gt_table[i];

		if (t->state != Ready)
			continue;

		total_tickets += gt_thread_ticket_count(t);
	}

	if (total_tickets == 0)
		return NULL;

	draw = gt_rand_u32() % total_tickets;

	for (i = 0; i < MaxGThreads; ++i) {
		struct gt *t = &gt_table[i];

		if (t->state != Ready)
			continue;

		cumulative += gt_thread_ticket_count(t);
		if (draw < cumulative)
			return t;
	}

	return NULL;
}

static void gt_age_ready_threads(const struct gt *selected){
	int i;

	for(i = 0;i<MaxGThreads;++i){
		struct gt *t = &gt_table[i];

		if (t->state != Ready || t == selected)
			continue;

		if (t->starvation_boost < GtPriorityLowest - GtPriorityHighest)
			t->starvation_boost++;
		t->starvation_rounds++;
	}
}

static int gt_effective_priority(const struct gt *t){
	int boosted = t-> priority - t-> starvation_boost;
	if (boosted < GtPriorityHighest)
		return GtPriorityHighest;
	return boosted;
}

static const char *gt_sched_policy_name(void) {
	switch (gt_sched_policy_current) {
	case GtSchedRR:
		return "RR";
	case GtSchedLS:
		return "LS";
	case GtSchedPRI:
	default:
		return "PRI";
	}
}

static void gt_handle_platform_tick(void) {
	scheduler_tick();
}

static void gt_handle_platform_sigint(void) {
	gt_dump_stats();
	gt_platform_exit(0);
}

static uint64_t gt_now_us(void) {
	return gt_platform_now_us();
}

static void gt_account_running_slice(struct gt *t, uint64_t now_us) {
	uint64_t delta;

	if (t->state != Running || t->last_run_start_us == 0)
		return;

	delta = now_us - t->last_run_start_us;
	t->run_total_us += delta;
	t->slice_count++;
	t->slice_sum_us += delta;
	t->slice_sum_sq_us += delta * delta;

	if (delta < t->slice_min_us)
		t->slice_min_us = delta;
	if (delta > t->slice_max_us)
		t->slice_max_us = delta;
}

static void gt_reset_thread_stats(struct gt *t, uint32_t tid, uint64_t now_us) {
	t->tid = tid;
	t->stack_base = NULL;
	t->entry = NULL;
	t->arg = NULL;
	t->exit_code = 0;
	t->priority = GtPriorityLowest;
	t->starvation_boost = 0;
	t->starvation_rounds = 0;
	t->last_state_change_us = now_us;
	t->last_run_start_us = 0;
	t->run_total_us = 0;
	t->wait_total_us = 0;
	t->schedule_count = 0;
	t->slice_count = 0;
	t->slice_min_us = UINT64_MAX;
	t->slice_max_us = 0;
	t->slice_sum_us = 0;
	t->slice_sum_sq_us = 0;
}

static const char *gt_state_name(int state) {
	switch (state) {
	case Unused:
		return "Unused";
	case Running:
		return "Running";
	case Ready:
		return "Ready";
	default:
		return "Unknown";
	}
}

static uint64_t gt_safe_min_slice(const struct gt *t) {
	if (t->slice_count == 0)
		return 0;
	return t->slice_min_us;
}

static uint64_t gt_avg_slice_us(const struct gt *t) {
	if (t->slice_count == 0)
		return 0;
	return t->slice_sum_us / t->slice_count;
}

static uint64_t gt_var_slice_us(const struct gt *t) {
	uint64_t avg;

	if (t->slice_count == 0)
		return 0;

	avg = gt_avg_slice_us(t);
	return (t->slice_sum_sq_us / t->slice_count) - (avg * avg);
}

static void __attribute__((noreturn)) gt_thread_trampoline(void) {
	gt_entry_fn_t entry = gt_current->entry;
	void *arg = gt_current->arg;

	if (entry)
		entry(arg);

	gt_exit(0);
	for (;;) {
	}
}

static bool gt_schedule_internal(void) {
	struct gt *p;
	struct gt_context *old, *new;
	uint64_t now_us = gt_now_us();

	gt_account_running_slice(gt_current, now_us);
	switch (gt_sched_policy_current) {
	case GtSchedRR:
		p = gt_find_next_ready_rr();
		break;
	case GtSchedLS:
		p = gt_find_next_ready_lottery();
		break;
	case GtSchedPRI:
	default:
		p = gt_find_next_ready_by_priority();
		break;
	}
	if (!p) {
		gt_current->last_run_start_us = now_us;
		return false;
	}

	if (gt_sched_policy_current == GtSchedPRI)
		gt_age_ready_threads(p);

	if (gt_current->state != Unused) {
		gt_current->state = Ready;
		gt_current->last_state_change_us = now_us;
		gt_current->last_run_start_us = 0;
	}

	p->wait_total_us += now_us - p->last_state_change_us;
	p->state = Running;
	p->schedule_count++;
	p->last_state_change_us = now_us;
	p->last_run_start_us = now_us;
	if (gt_sched_policy_current == GtSchedPRI)
		p->starvation_boost = 0;

	old = &gt_current->ctx;
	new = &p->ctx;
	gt_current = p;
	gt_switch(old, new);
	return true;
}

void gt_dump_stats(void) {
	int i;
	uint64_t now_us = gt_now_us();

	gt_platform_printf("\n=== gthreads stats ===\n");
	gt_platform_printf(" scheduler = %s\n", gt_sched_policy_name());
	gt_platform_printf(" tid | state   | base_prio | eff_prio | tickets | boost | run_us | wait_us | switches | slices | min_us | max_us | avg_us | var_us\n");
	gt_platform_printf("-----+---------+-----------+----------+---------+-------+--------+---------+----------+--------+--------+--------+--------+--------\n");

	for (i = 0; i < MaxGThreads; ++i) {
		struct gt *t = &gt_table[i];
		uint64_t run = t->run_total_us;
		uint64_t wait = t->wait_total_us;

		if (t->state == Running && t->last_run_start_us != 0)
			run += now_us - t->last_run_start_us;
		if (t->state == Ready)
			wait += now_us - t->last_state_change_us;

		gt_platform_printf(" %3u | %-7s | %9d | %8d | %7u | %5d | %6llu | %7llu | %8llu | %6llu | %6llu | %6llu | %6llu | %6llu\n",
			(unsigned)t->tid,
			gt_state_name(t->state),
			t->priority,
			gt_effective_priority(t),
			gt_thread_ticket_count(t),
			t->starvation_boost,
			(unsigned long long)run,
			(unsigned long long)wait,
			(unsigned long long)t->schedule_count,
			(unsigned long long)t->slice_count,
			(unsigned long long)gt_safe_min_slice(t),
			(unsigned long long)t->slice_max_us,
			(unsigned long long)gt_avg_slice_us(t),
			(unsigned long long)gt_var_slice_us(t));
	}
}

void gt_init(void) {
	uint64_t now_us = gt_now_us();
	int i;

	gt_rng_state = now_us ^ 0x6a09e667f3bcc909ULL;

	for (i = 0; i < MaxGThreads; ++i) {
		gt_table[i].state = Unused;
		gt_reset_thread_stats(&gt_table[i], (uint32_t)i, now_us);
	}

	gt_current = &gt_table[0];
	gt_current->state = Running;
	gt_current->last_run_start_us = now_us;
	gt_current->last_state_change_us = now_us;

	gt_platform_install_periodic_tick(500, 500, gt_handle_platform_tick);
	gt_platform_install_sigint(gt_handle_platform_sigint);
}

void scheduler_init(void) {
	gt_init();
}

void gt_set_scheduler(enum gt_sched_policy policy) {
	switch (policy) {
	case GtSchedRR:
	case GtSchedPRI:
	case GtSchedLS:
		gt_sched_policy_current = policy;
		break;
	default:
		gt_sched_policy_current = GtSchedPRI;
		break;
	}
}

void scheduler_tick(void) {
	if (gt_in_schedule)
		return;

	gt_in_schedule = true;
	gt_schedule();
	gt_in_schedule = false;
}

void __attribute__((noreturn)) gt_exit(int code) {
	uint64_t now_us = gt_now_us();

	if (gt_current != &gt_table[0]) {
		gt_account_running_slice(gt_current, now_us);
		gt_current->exit_code = code;
		gt_current->state = Unused;
		gt_current->last_state_change_us = now_us;
		gt_current->last_run_start_us = 0;
		gt_current->entry = NULL;
		gt_current->arg = NULL;
		gt_current->stack_base = NULL;
		gt_schedule_internal();
		for (;;) {
		}
	}

	while (gt_schedule_internal()) {
	}
	gt_platform_exit(code);
}

void __attribute__((noreturn)) gt_return(int ret) {
	gt_exit(ret);
}

void gt_schedule(void) {
	(void)gt_schedule_internal();
}

void gt_yield(void) {
	gt_schedule();
}

// return function for terminating thread
void gt_stop(void) {
	gt_exit(0);
}

int gt_create(gt_entry_fn_t entry, void *arg, int priority) {
	uint8_t *stack;
	struct gt *p;
	uint64_t now_us;

	for (p = &gt_table[0];; p++) {
		if (p == &gt_table[MaxGThreads])
			return -1;
		if (p->state == Unused && p != &gt_table[0])
			break;
	}

	now_us = gt_now_us();
	gt_reset_thread_stats(p, (uint32_t)(p - &gt_table[0]), now_us);

	stack = gt_stacks[p->tid];
	*(uint64_t *)&stack[StackSize - 8] = (uint64_t)gt_thread_trampoline;

	p->ctx.rsp = (uint64_t)&stack[StackSize - 8];
	p->stack_base = stack;
	p->entry = entry;
	p->arg = arg;
	p->priority = gt_clamp_priority(priority);
	p->starvation_boost = 0;
	p->starvation_rounds = 0;
	p->state = Ready;
	p->last_state_change_us = now_us;

	return 0;
}

int gt_uninterruptible_nanosleep(time_t sec, long nanosec) {
	return gt_platform_uninterruptible_nanosleep(sec, nanosec);
}
