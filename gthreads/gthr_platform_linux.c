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

uint64_t gt_platform_now_us(void) {
	struct timeval tv;

	/* gettimeofday(2): tz should be NULL; return -1 on error */
	if (gettimeofday(&tv, NULL) != 0)
		return 0;

	return ((uint64_t)tv.tv_sec * 1000000ULL) + (uint64_t)tv.tv_usec;
}

void *gt_platform_alloc(size_t size) {
	return malloc(size);
}

void gt_platform_free(void *ptr) {
	free(ptr);
}

void gt_platform_exit(int code) {
	exit(code);
}

void gt_platform_register_signal(int sig, gt_signal_handler_t handler) {
	signal(sig, handler);
}

void gt_platform_unblock_signal(int sig) {
	#ifdef SIG_UNBLOCK
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, sig);
	sigprocmask(SIG_UNBLOCK, &set, NULL);
	#else
	(void) sig;
	#endif
}

void gt_platform_clear_alarm(void) {
	alarm(0);
}

void gt_platform_set_periodic_alarm_us(unsigned int start_us, unsigned int interval_us) {
	ualarm(start_us, interval_us);
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
