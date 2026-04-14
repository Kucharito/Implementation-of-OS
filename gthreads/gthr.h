#include <stdbool.h>
#include <stdint.h>
#include <sys/time.h>
#include <time.h>

enum {
	MaxGThreads = 5,            // Maximum number of threads, used as array size for gttbl
	StackSize = 0x400000,       // Size of stack of each thread
};

typedef void (*gt_entry_fn_t)(void *);

struct gt_context {
	uint64_t rsp;
	uint64_t r15;
	uint64_t r14;
	uint64_t r13;
	uint64_t r12;
	uint64_t rbx;
	uint64_t rbp;
};

struct gt {
	// Saved context, switched by gtswtch.S (see for detail)
	struct gt_context ctx;
	// Thread state
	enum {
		Unused,
		Running,
		Ready,
	}
	state;

	//identity + memory
	uint32_t tid;
	void *stack_base;
	gt_entry_fn_t entry;
	void *arg;
	int exit_code;

	//state timing
	uint64_t last_state_change_us;
	uint64_t last_run_start_us;

	//cumulative totals
	uint64_t run_total_us;
	uint64_t wait_total_us;

	//schedule counters
	uint64_t schedule_count;
	uint64_t slice_count;

	//slice time counters
	uint64_t slice_min_us;
	uint64_t slice_max_us;
	uint64_t slice_sum_us;
	uint64_t slice_sum_sq_us;
};
void gt_init(void);                                                     // initialize gttbl
void scheduler_init(void);                                              // scheduler alias for kernel integration
void scheduler_tick(void);                                              // timer tick hook for kernel integration
void gt_switch(struct gt_context * old, struct gt_context * new);       // declaration from gtswtch.S
void gt_schedule(void);                                                  // yield and switch to another thread
void gt_yield(void);                                                     // cooperative yield helper
void gt_exit(int code);                                                  // terminate current thread in controlled way
void gt_return(int ret);                                                 // compatibility wrapper around gt_exit
void gt_stop(void);                                                     // terminate current thread
int gt_create(gt_entry_fn_t entry, void *arg);                           // create new thread and set entry function + argument
void gt_reset_sig(int sig);                                             // resets signal
void gt_alarm_handle(int sig);                                          // periodically triggered by alarm
int gt_uninterruptible_nanosleep(time_t sec, long nanosec);             // uninterruptible sleep
void gt_sigint_handle(int sig);                                          // handle SIGINT signal (Ctrl+C)
void gt_dump_stats(void);                                                // dump thread statistics