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
#include <stdlib.h>
#include <unistd.h>

#include <sys/threads.h>
#include <sys/time.h>


#define THREAD_STACK_SIZE 1024
#define JITTER_SAMPLES    5000


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


#define TEST_REPORT(s, thr) \
	do { \
		printf("TEST %s RESULTS (%d thread%s):\n", s, thr, (thr > 1) ? "s" : ""); \
	} while (0)


static inline uint64_t getCntr(void)
{
	uint32_t asr22, asr23;
	uint64_t cntr;
	__asm__ volatile(
			"rd %%asr22, %0\n\t"
			"rd %%asr23, %1\n\t"
			: "=r"(asr22), "=r"(asr23) :);

	cntr = ((uint64_t)(asr22 & 0xffffffu) << 32) | asr23;

	return cntr;
}


static struct {
	unsigned int counters[1024];
	unsigned int bgcounters[1024];

	uint64_t jitter[JITTER_SAMPLES];

	atomic_int taskStart;
	atomic_int taskEnd;

	uint8_t stack[1024][THREAD_STACK_SIZE];
	uint8_t bgstack[16][THREAD_STACK_SIZE];
} common;


/******************************************************
 *                  Benchmark tasks                   *
 ******************************************************/


void loopTask(void *arg)
{
	unsigned int n = (unsigned int)arg;
	while (!common.taskStart) {
		usleep(0);
	}

	while (!common.taskEnd) {
		unsigned int x = common.counters[n];
		unsigned int v = (unsigned int)&x;
		v = (v << 13) ^ v;
		v = x * (v * v * 15731 + 789221) + 1376312589;
		v /= 152;

		++common.counters[n];
	}

	endthread();
}


void jitterTask(void *arg)
{
	while (!common.taskStart) {
		usleep(0);
	}

	for (int i = 0; (i < JITTER_SAMPLES) && !common.taskEnd; i++) {
		uint64_t start = getCntr();
		usleep(1000);
		uint64_t end = getCntr();

		common.jitter[i] = end - start;
	}

	endthread();
}


void idleTask(void *arg)
{
	unsigned int n = (unsigned int)arg;
	while (!common.taskStart) {
		usleep(0);
	}

	while (!common.taskEnd) {
		++common.bgcounters[n];
	}

	endthread();
}


static const int allocationSizes[] = { 3, 31, 16, 128, 1, 2015, 30, 290, 100, 2, 496, 1531 };


void task3(void *arg)
{
	while (!common.taskStart) {
		usleep(0);
	}

	for (int i = 0; i < 1000; i++) {
		void *ptr;
		for (int j = 0; j < sizeof(allocationSizes) / sizeof(allocationSizes[0]); j++) {
			ptr = malloc(allocationSizes[j]);
			if (ptr == NULL) {
				fprintf(stderr, "malloc failed\n");
				endthread();
			}
			free(ptr);
		}
	}

	endthread();
}


/******************************************************
 *                        Main                        *
 ******************************************************/


int doTest(void (*task)(void *), int ntasks, void (*bgTask)(void *), int nbgTasks)
{
	common.taskStart = false;
	common.taskEnd = false;

	static int tid[1024];

	for (int i = 0; i < ntasks; i++) {
		ERROR_CHECK(beginthreadex(task, 2, common.stack[i], sizeof(common.stack[i]), (void *)i, &tid[i]));
	}

	static int bgTid[16];

	if (nbgTasks > 16) {
		nbgTasks = 16;
	}

	for (int i = 0; i < nbgTasks; i++) {
		ERROR_CHECK(beginthreadex(bgTask, 3, common.bgstack[i], sizeof(common.bgstack[i]), (void *)i, &bgTid[i]));
	}

	common.taskStart = true;

	usleep(5 * 1000 * 1000);

	common.taskEnd = true;

	for (int i = 0; i < ntasks; i++) {
		ERROR_CHECK(threadJoin(tid[i], 0));
	}

	for (int i = 0; i < nbgTasks; i++) {
		ERROR_CHECK(threadJoin(bgTid[i], 0));
	}


	return 0;
}


int main(int argc, char *argv[])
{
	int ntasks = 750;

	if (argc > 1) {
		ntasks = atoi(argv[1]);
		if (ntasks > 1024) {
			ntasks = 1024;
			printf("Number of tasks limited to 1024\n");
		}
	}

	ERROR_CHECK(priority(0));

	ERROR_CHECK(doTest(idleTask, ntasks, NULL, 0));

	TEST_REPORT("High load", ntasks);
	for (int i = 0; i < ntasks; i++) {
		printf("%d%c", common.bgcounters[i], (i == ntasks - 1) ? '\n' : ' ');
	}

	// ERROR_CHECK(doTest(jitterTask, 1, NULL, 0));

	// TEST_REPORT("Jitter", 1);
	// for (int i = 0; i < JITTER_SAMPLES; i++) {
	// 	printf("%llu%c", common.jitter[i], (i == JITTER_SAMPLES - 1) ? '\n' : ',');
	// }

	// ERROR_CHECK(doTest(jitterTask, 1, idleTask, 4));

	// TEST_REPORT("Jitter with idle", 5);
	// for (int i = 0; i < JITTER_SAMPLES; i++) {
	// 	printf("%llu%c", common.jitter[i], (i == JITTER_SAMPLES - 1) ? '\n' : ',');
	// }
}
