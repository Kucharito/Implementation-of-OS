#ifndef GTHR_PLATFORM_H
#define GTHR_PLATFORM_H

#include <stdarg.h>
#include <stdint.h>
#include <time.h>

typedef void (*gt_platform_callback_t)(void);

uint64_t gt_platform_now_us(void);
void gt_platform_exit(int code) __attribute__((noreturn));
void gt_platform_install_periodic_tick(unsigned int start_us, unsigned int interval_us, gt_platform_callback_t cb);
void gt_platform_install_sigint(gt_platform_callback_t cb);

int gt_platform_uninterruptible_nanosleep(time_t sec, long nanosec);

void gt_platform_printf(const char *fmt, ...);

#endif
