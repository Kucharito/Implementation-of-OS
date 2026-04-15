#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "gthr_platform.h"

static gt_platform_callback_t gt_tick_cb;
static gt_platform_callback_t gt_sigint_cb;

static void gt_platform_alarm_handler(int sig) {
	(void)sig;
	if (gt_tick_cb)
		gt_tick_cb();
}

static void gt_platform_sigint_handler(int sig) {
	(void)sig;
	if (gt_sigint_cb)
		gt_sigint_cb();
}

uint64_t gt_platform_now_us(void) {
	struct timeval tv;

	if (gettimeofday(&tv, NULL) != 0)
		return 0;

	return ((uint64_t)tv.tv_sec * 1000000ULL) + (uint64_t)tv.tv_usec;
}

void gt_platform_exit(int code) {
	exit(code);
}

void gt_platform_install_periodic_tick(unsigned int start_us, unsigned int interval_us, gt_platform_callback_t cb) {
	gt_tick_cb = cb;
	signal(SIGALRM, gt_platform_alarm_handler);
	ualarm(start_us, interval_us);
}

void gt_platform_install_sigint(gt_platform_callback_t cb) {
	gt_sigint_cb = cb;
	signal(SIGINT, gt_platform_sigint_handler);
}

int gt_platform_uninterruptible_nanosleep(time_t sec, long nanosec) {
	struct timespec req;
	req.tv_sec = sec;
	req.tv_nsec = nanosec;

	do {
		if (nanosleep(&req, &req) != 0) {
			if (errno != EINTR)
				return -1;
		} else {
			break;
		}
	} while (req.tv_sec > 0 || req.tv_nsec > 0);

	return 0;
}

void gt_platform_printf(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
}
