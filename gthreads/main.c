// Based on https://c9x.me/articles/gthreads/code0.html
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <time.h>

#include "gthr.h"

// Dummy function to simulate some thread work
void f(void *arg) {
	int i = 0, id;

	id = (int)(uintptr_t)arg;
	while (true) {

		printf("F Thread id = %d, val = %d BEGINNING\n", id, ++i);
		gt_uninterruptible_nanosleep(0, 50000000);
		printf("F Thread id = %d, val = %d END\n", id, ++i);
		gt_uninterruptible_nanosleep(0, 50000000);
	}
}

// Dummy function to simulate some thread work
void g(void *arg) {
	int i = 0, id;

	id = (int)(uintptr_t)arg;
	while (true) {
		printf("G Thread id = %d, val = %d BEGINNING\n", id, ++i);
		gt_uninterruptible_nanosleep(0, 50000000);
		printf("G Thread id = %d, val = %d END\n", id, ++i);
		gt_uninterruptible_nanosleep(0, 50000000);
	}
}

int main(void) {
	gt_init();            // initialize threads, see gthr.c
	gt_create(f, (void *)(uintptr_t)1);         // set f() as first thread
	gt_create(f, (void *)(uintptr_t)2);         // set f() as second thread
	gt_create(g, (void *)(uintptr_t)1);         // set g() as third thread
	gt_create(g, (void *)(uintptr_t)2);         // set g() as fourth thread
	gt_exit(1);           // wait until all threads terminate
}
