#ifndef GTHR_PLATFORM_H
#define GTHR_PLATFORM_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

typedef void (*gt_signal_handler_t)(int);

uint64_t gt_platform_now_us(void);
void *gt_platform_alloc(size_t size);
void gt_platform_free(void *ptr);
void gt_platform_exit(int code) __attribute__((noreturn));

void gt_platform_register_signal(int sig, gt_signal_handler_t handler);
void gt_platform_unblock_signal(int sig);
void gt_platform_clear_alarm(void);
void gt_platform_set_periodic_alarm_us(unsigned int start_us, unsigned int interval_us);

int gt_platform_uninterruptible_nanosleep(time_t sec, long nanosec);

void gt_platform_printf(const char *fmt, ...);

#endif
