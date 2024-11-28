/*
 * Phoenix-RTOS
 *
 * High load benchmark
 *
 * Copyright 2024 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/threads.h>
#include <sys/time.h>


extern int nsleep(time_t *sec, long *nsec);


#define ERROR_CHECK(x) \
	do { \
		int _rc = (x); \
		if (_rc < 0) { \
			fprintf(stderr, #x ": error %d\n", _rc); \
			return -1; \
		} \
	} while (0)

#define ERROR_CHECK_THR(x) \
	do { \
		int _rc = (x); \
		if (_rc < 0) { \
			fprintf(stderr, #x ": error %d\n", _rc); \
			endthread(); \
		} \
	} while (0)


static struct {
	handle_t taskEndCond;
	handle_t timerCond[2];
	int timerLoop[2];

	atomic_bool taskEnd;
	unsigned int idleTimes[4];
	uint8_t stack[11][4096];
	uint8_t idleStack[4][1024];
	uint8_t timerStack[2][2048];
} common = {
	.taskEnd = false
};


/******************************************************
 *                Infrastructure tasks                *
 ******************************************************/
time_t saved[2], end[2];

void timer(void *arg)
{
	unsigned int n = (unsigned int)arg;

	time_t interval;

	if (n == 0) {
		interval = 200;
	}
	else if (n == 1) {
		interval = 1000;
	}
	else {
		endthread();
	}

	handle_t timerCond = common.timerCond[n];
	handle_t waitCond, waitMut;
	ERROR_CHECK_THR(condCreateWithAttr(&waitCond, &(const struct condAttr) { .clock = PH_CLOCK_MONOTONIC }));
	ERROR_CHECK_THR(mutexCreate(&waitMut));

	mutexLock(waitMut);

	time_t now;
	(void)gettime(&now, NULL);
	saved[n] = now;


	while (!common.taskEnd) {
		now += interval;
		condWait(waitCond, waitMut, now);
		condBroadcast(timerCond);
		common.timerLoop[n]++;
		if ((n == 0) && (common.timerLoop[n] == 10000)) {
			common.taskEnd = true;
		}
	}
	gettime(&end[n], NULL);
	mutexUnlock(waitMut);
	endthread();
}


/******************************************************
 *                  Benchmark tasks                   *
 ******************************************************/


void task1(void *arg)
{
	endthread();
}


void idleTask(void *arg)
{
	unsigned int i = (unsigned int)arg;
	while (!common.taskEnd) {
		common.idleTimes[i]++;
	}
	endthread();
}


/******************************************************
 *                        Main                        *
 ******************************************************/


int main(void)
{
	int tid[15];
	int idleTid[4];

	ERROR_CHECK(priority(0));
	ERROR_CHECK(condCreate(&common.taskEndCond));

	/* Run 4 idle tasks as we have 4 cores */
	for (int i = 0; i < 4; i++) {
		ERROR_CHECK(beginthreadex(idleTask, 6, common.idleStack[i], sizeof(common.idleStack[i]), (void *)i, &idleTid[i]));
	}

	/* Run software timers */
	for (int i = 0; i < 2; i++) {
		ERROR_CHECK(beginthreadex(timer, 1, common.timerStack[i], sizeof(common.timerStack[i]), (void *)i, &tid[i]));
	}

	// for (int i = 0; i < 11; i++) {
	// 	ERROR_CHECK(threadJoin(tid[i], 0));
	// }

	// usleep(2 * 1000 * 1000);

	// common.taskEnd = true;

	for (int i = 0; i < 2; i++) {
		ERROR_CHECK(threadJoin(tid[i], 0));
	}

	for (int i = 0; i < 4; i++) {
		ERROR_CHECK(threadJoin(idleTid[i], 0));
	}

	/* Report results */
	for (int i = 0; i < 4; i++) {
		printf("Idle task %d: %u\n", i, common.idleTimes[i]);
	}
	printf("Sum of idle times: %u\n", common.idleTimes[0] + common.idleTimes[1] + common.idleTimes[2] + common.idleTimes[3]);
    printf("Timer 0 time: %llu\n", end[0] - saved[0]);
    printf("Timer 1 time: %llu\n", end[1] - saved[1]);
}
