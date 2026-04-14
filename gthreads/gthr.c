#include <assert.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "gthr.h"
#include "gthr_platform.h"
#include "gthr_struct.h"

static volatile sig_atomic_t gt_stats_requested;
static uint8_t gt_stacks[MaxGThreads][StackSize];

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
	assert(!"reachable");
}

static bool gt_schedule_internal(void) {
	struct gt *p;
	struct gt_context *old, *new;
	uint64_t now_us = gt_now_us();

	if (gt_stats_requested) {
		gt_dump_stats();
		gt_stats_requested = 0;
	}

	gt_account_running_slice(gt_current, now_us);
	gt_reset_sig(SIGALRM);

	p = gt_current;
	while (p->state != Ready) {
		if (++p == &gt_table[MaxGThreads])
			p = &gt_table[0];
		if (p == gt_current) {
			gt_current->last_run_start_us = now_us;
			return false;
		}
	}

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

	old = &gt_current->ctx;
	new = &p->ctx;
	gt_current = p;
	gt_switch(old, new);
	return true;
}

// function triggered periodically by timer (SIGALRM)
void gt_alarm_handle(int sig) {
	(void)sig;
	scheduler_tick();
}

// SIGINT handler only marks that a dump should be printed
void gt_sigint_handle(int sig) {
	(void)sig;
	gt_stats_requested = 1;
	gt_dump_stats();
	gt_stats_requested = 0;
	gt_platform_exit(0);
}

void gt_dump_stats(void) {
	int i;
	uint64_t now_us = gt_now_us();

	gt_platform_printf("\n=== gthreads stats ===\n");
	gt_platform_printf(" tid | state   | run_us | wait_us | switches | slices | min_us | max_us | avg_us | var_us\n");
	gt_platform_printf("-----+---------+--------+---------+----------+--------+--------+--------+--------+--------\n");

	for (i = 0; i < MaxGThreads; ++i) {
		struct gt *t = &gt_table[i];
		uint64_t run = t->run_total_us;
		uint64_t wait = t->wait_total_us;

		if (t->state == Running && t->last_run_start_us != 0)
			run += now_us - t->last_run_start_us;
		if (t->state == Ready)
			wait += now_us - t->last_state_change_us;

		gt_platform_printf(" %3u | %-7s | %6llu | %7llu | %8llu | %6llu | %6llu | %6llu | %6llu | %6llu\n",
			(unsigned)t->tid,
			gt_state_name(t->state),
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

	for (i = 0; i < MaxGThreads; ++i) {
		gt_table[i].state = Unused;
		gt_reset_thread_stats(&gt_table[i], (uint32_t)i, now_us);
	}

	gt_current = &gt_table[0];
	gt_current->state = Running;
	gt_current->last_run_start_us = now_us;
	gt_current->last_state_change_us = now_us;

	gt_platform_register_signal(SIGALRM, gt_alarm_handle);
	gt_platform_register_signal(SIGINT, gt_sigint_handle);
	gt_reset_sig(SIGALRM);
}

void scheduler_init(void) {
	gt_init();
}

void scheduler_tick(void) {
	gt_schedule();
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
		assert(!"reachable");
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

int gt_create(gt_entry_fn_t entry, void *arg) {
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
	p->state = Ready;
	p->last_state_change_us = now_us;

	return 0;
}

// resets SIGALRM signal
void gt_reset_sig(int sig) {
	if (sig == SIGALRM) {
		gt_platform_clear_alarm();
	}

	gt_platform_unblock_signal(sig);

	if (sig == SIGALRM) {
		gt_platform_set_periodic_alarm_us(500, 500);
	}
}

int gt_uninterruptible_nanosleep(time_t sec, long nanosec) {
	return gt_platform_uninterruptible_nanosleep(sec, nanosec);
}
