// Based on https://c9x.me/articles/gthreads/code0.html
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <time.h>

#include "gthr.h"

// Dummy function to simulate some thread work
void f(void *arg) {
	int i = 0, id;

	id = (int)(uintptr_t)arg;
	while (true) {

		printf("F Thread id = %d, val = %d BEGINNING\n", id, ++i);
		gt_yield();
		gt_uninterruptible_nanosleep(0, 50000000);
		printf("F Thread id = %d, val = %d END\n", id, ++i);
		gt_yield();
		gt_uninterruptible_nanosleep(0, 50000000);
	}
}

// Dummy function to simulate some thread work
void g(void *arg) {
	int i = 0, id;

	id = (int)(uintptr_t)arg;
	while (true) {
		printf("G Thread id = %d, val = %d BEGINNING\n", id, ++i);
		gt_yield();
		gt_uninterruptible_nanosleep(0, 50000000);
		printf("G Thread id = %d, val = %d END\n", id, ++i);
		gt_yield();
		gt_uninterruptible_nanosleep(0, 50000000);
	}
}

int main(int argc, char **argv) {
	enum gt_sched_policy policy = GtSchedPRI;

	if (argc > 1) {
		if (strcmp(argv[1], "RR") == 0) {
			policy = GtSchedRR;
		} else if (strcmp(argv[1], "PRI") == 0) {
			policy = GtSchedPRI;
		} else if (strcmp(argv[1], "LS") == 0) {
			policy = GtSchedLS;
		} else {
			fprintf(stderr, "Unknown scheduler '%s' (use RR | PRI | LS)\n", argv[1]);
			return 2;
		}
	}

	gt_set_scheduler(policy);
	scheduler_init();            // initialize scheduler and thread table
	gt_create(f, (void *)(uintptr_t)1, 0);      // highest priority
	gt_create(f, (void *)(uintptr_t)2, 1);
	gt_create(g, (void *)(uintptr_t)1, 2);
	gt_create(g, (void *)(uintptr_t)2, 4);
	gt_create(f, (void *)(uintptr_t)3, 6);
	gt_create(g, (void *)(uintptr_t)3, 8);
	gt_create(f, (void *)(uintptr_t)4, 10);     // lowest priority
	gt_return(1);           // wait until all threads terminate
}
