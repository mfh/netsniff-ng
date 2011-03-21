/*
 * netsniff-ng - the packet sniffing beast
 * By Daniel Borkmann <daniel@netsniff-ng.org>
 * Copyright 2009, 2010 Daniel Borkmann.
 * Subject to the GPL.
 */

#ifndef TIMESPEC_H
#define TIMESPEC_H

#include <limits.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <stdbool.h>

#define TYPE_SIGNED(t) (! ((t) 0 < (t) -1))
#define TYPE_MAXIMUM(t)                                  \
  ((t) (! TYPE_SIGNED(t)                                 \
	? (t) -1                                         \
	: ~ (~ (t) 0 << (sizeof(t) * CHAR_BIT - 1))))

#ifndef TIME_T_MAX
# define TIME_T_MAX TYPE_MAXIMUM(time_t)
#endif

static inline int timespec_subtract(struct timespec *result, 
				    struct timespec *after,
				    struct timespec *before)
{
	result->tv_nsec = after->tv_nsec - before->tv_nsec;

	if (result->tv_nsec < 0) {
		/* Borrow 1sec from 'tv_sec' if subtraction -ve */
		result->tv_nsec += 1000000000;
		result->tv_sec = after->tv_sec - before->tv_sec - 1;

		return 1;
	} else {
		result->tv_sec = after->tv_sec - before->tv_sec;
		return 0;
	}
}

/* By Jim Meyering */
static inline void xusleep(const struct timespec *ts_delay)
{
	struct timeval tv_delay;

	tv_delay.tv_sec = ts_delay->tv_sec;
	tv_delay.tv_usec = (ts_delay->tv_nsec + 999) / 1000;

	if (tv_delay.tv_usec == 1000000) {
		time_t t1 = tv_delay.tv_sec + 1;
		if (t1 < tv_delay.tv_sec)
			tv_delay.tv_usec = 1000000 - 1; /* Close enough */
		else {
			tv_delay.tv_sec = t1;
			tv_delay.tv_usec = 0;
		}
	}

	select(0, NULL, NULL, NULL, &tv_delay);
}

/* By Paul Eggert, Jim Meyering */
static inline int xnanosleep(double seconds)
{
	enum {
		BILLION = 1000000000
	};

	bool overflow = false;
	double ns;
	struct timespec ts_sleep;

	assert(0 <= seconds);

	/*
	 * Separate whole seconds from nanoseconds.
	 * Be careful to detect any overflow.
	 */

	ts_sleep.tv_sec = seconds;
	ns = BILLION * (seconds - ts_sleep.tv_sec);
	overflow |= !(ts_sleep.tv_sec <= seconds && 0 <= ns && ns <= BILLION);
	ts_sleep.tv_nsec = ns;

	/*
	 * Round up to the next whole number, if necessary, so that we
	 * always sleep for at least the requested amount of time. Assuming
	 * the default rounding mode, we don't have to worry about the
	 * rounding error when computing 'ns' above, since the error won't
	 * cause 'ns' to drop below an integer boundary.
	 */

	ts_sleep.tv_nsec += (ts_sleep.tv_nsec < ns);

	/* Normalize the interval length. nanosleep requires this. */

	if (BILLION <= ts_sleep.tv_nsec) {
		time_t t = ts_sleep.tv_sec + 1;

		/* Detect integer overflow.  */
		overflow |= (t < ts_sleep.tv_sec);

		ts_sleep.tv_sec = t;
		ts_sleep.tv_nsec -= BILLION;
	}

	for (;;) {
		if (overflow) {
			ts_sleep.tv_sec = TIME_T_MAX;
			ts_sleep.tv_nsec = BILLION - 1;
		}

		/*
		 * Linux-2.6.8.1's nanosleep returns -1, but doesn't set errno
		 * when resumed after being suspended.  Earlier versions would
		 * set errno to EINTR.  nanosleep from linux-2.6.10, as well as
		 * implementations by (all?) other vendors, doesn't return -1
		 * in that case;  either it continues sleeping (if time remains)
		 * or it returns zero (if the wake-up time has passed).
		 */

		errno = 0;

		if (nanosleep(&ts_sleep, NULL) == 0)
			break;
		if (errno != EINTR && errno != 0)
			return -1;
	}

	return 0;
}

#endif /* TIMESPEC_H */