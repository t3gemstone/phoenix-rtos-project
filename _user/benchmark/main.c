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
#include <errno.h>

#include <sys/threads.h>
#include <sys/time.h>


#define THREAD_STACK_SIZE 1024
#define JITTER_SAMPLES    5000
#define MAX_TASKS         1024
#define MAX_BG_TASKS      256
#define MAX_JITTER_TASKS  4


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

#define MALLOC_SAMPLES 1000

static struct {
	unsigned int loopCounters[MAX_TASKS];
	unsigned int idleCounters[MAX_BG_TASKS];

	uint64_t jitter[MAX_JITTER_TASKS][JITTER_SAMPLES];
	handle_t jitterCond[MAX_JITTER_TASKS];
	handle_t jitterMutex[MAX_JITTER_TASKS];

	uint64_t mallocTimes[MALLOC_SAMPLES];

	volatile int taskStart;
	volatile int taskEnd;

	uint8_t stack[MAX_TASKS][THREAD_STACK_SIZE];
	uint8_t bgstack[MAX_BG_TASKS][THREAD_STACK_SIZE];
} common;


#define BENCHMARK_DURATION_SEC 10

/******************************************************
 *                  Benchmark tasks                   *
 ******************************************************/


void loopTask(void *arg)
{
	unsigned int n = (unsigned int)arg;
	while (!common.taskStart) {
		usleep(0);
	}

	uint64_t start_cycles = getCntr();
	uint64_t end_cycles = start_cycles + (BENCHMARK_DURATION_SEC * 250000000ULL);  // 250 MHz * 1 second

	while (getCntr() < end_cycles) {
		volatile unsigned int x = common.loopCounters[n];
		volatile unsigned int v = x + 16;
		v = (v << 13) ^ v;
		v = x * (v * v * 15731 + 789221) + 1376312589;
		v /= 152;

		++common.loopCounters[n];
	}

	endthread();
}


void jitterTask(void *arg)
{
	unsigned int n = (unsigned int)arg;
	time_t sleep;

	switch (n) {
		case 0:
			sleep = 1000;
			break;
		case 1:
			sleep = 1400;
			break;
		case 2:
			sleep = 1800;
			break;
		case 3:
			sleep = 2200;
			break;
		default:
			endthread();
			break;
	}

	mutexLock(common.jitterMutex[n]);

	while (!common.taskStart) {
		usleep(0);
	}

	time_t now;
	gettime(&now, NULL);
	for (int i = 0; (i < JITTER_SAMPLES) && !common.taskEnd; i++) {
		now += sleep;
		uint64_t start = getCntr();
		condWait(common.jitterCond[n], common.jitterMutex[n], now);
		uint64_t end = getCntr();

		common.jitter[n][i] = end - start;
	}

	mutexUnlock(common.jitterMutex[n]);

	endthread();
}


void idleTask(void *arg)
{
	unsigned int n = (unsigned int)arg;
	while (!common.taskStart) {
		usleep(0);
	}
	uint64_t start_cycles = getCntr();
	uint64_t end_cycles = start_cycles + (BENCHMARK_DURATION_SEC * 250000000ULL);  // 250 MHz * 1 second

	while (getCntr() < end_cycles) {
		++common.idleCounters[n];
	}

	endthread();
}


/******************************************************
 *                        Main                        *
 ******************************************************/


int doTest(void (*task)(void *), int ntasks, void (*bgTask)(void *), int nbgTasks, unsigned int sleepTimeSec)
{
	common.taskStart = false;
	common.taskEnd = false;

	static int tid[MAX_TASKS];

	for (int i = 0; i < ntasks; i++) {
		ERROR_CHECK(beginthreadex(task, 2, common.stack[i], sizeof(common.stack[i]), (void *)i, &tid[i]));
	}

	static int bgTid[MAX_BG_TASKS];

	if (nbgTasks > MAX_BG_TASKS) {
		nbgTasks = MAX_BG_TASKS;
	}

	for (int i = 0; i < nbgTasks; i++) {
		ERROR_CHECK(beginthreadex(bgTask, 3, common.bgstack[i], sizeof(common.bgstack[i]), (void *)i, &bgTid[i]));
	}

	common.taskStart = true;

	usleep(sleepTimeSec * 1000 * 1000);

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
	int ntasks = 4;
	printf("Starting benchmark\n");

	if (argc > 1) {
		ntasks = atoi(argv[1]);
		if (ntasks > MAX_TASKS) {
			ntasks = MAX_TASKS;
			printf("Number of tasks limited to %d\n", MAX_TASKS);
		}
	}

	// for (int i = 0; i < MAX_JITTER_TASKS; i++) {
	// 	struct condAttr attr = { .clock = PH_CLOCK_MONOTONIC };
	// 	ERROR_CHECK(condCreateWithAttr(&common.jitterCond[i], &attr));
	// 	ERROR_CHECK(mutexCreate(&common.jitterMutex[i]));
	// }

	ERROR_CHECK(priority(0));

	ERROR_CHECK(doTest(idleTask, ntasks, NULL, 0, 15));

	TEST_REPORT("High load", ntasks);
	for (int i = 0; i < ntasks; i++) {
		printf("%d%c", common.idleCounters[i], (i == ntasks - 1) ? '\n' : ',');
	}


	// ERROR_CHECK(doTest(jitterTask, 1, NULL, 0, 10));

	// TEST_REPORT("Jitter", 1);
	// for (int i = 0; i < JITTER_SAMPLES; i++) {
	// 	printf("%llu%c", common.jitter[0][i], (i == JITTER_SAMPLES - 1) ? '\n' : ',');
	// }

	// ERROR_CHECK(doTest(jitterTask, 1, idleTask, 10, 10));

	// TEST_REPORT("Jitter with idle", 11);
	// for (int i = 0; i < JITTER_SAMPLES; i++) {
	// 	printf("%llu%c", common.jitter[0][i], (i == JITTER_SAMPLES - 1) ? '\n' : ',');
	// }

	// ERROR_CHECK_THR(doTest(jitterTask, 1, idleTask, 256, 10));

	// TEST_REPORT("Jitter with idle", 257);
	// for (int i = 0; i < JITTER_SAMPLES; i++) {
	// 	printf("%llu%c", common.jitter[0][i], (i == JITTER_SAMPLES - 1) ? '\n' : ',');
	// }

	// ERROR_CHECK_THR(doTest(jitterTask, 4, idleTask, 256, 15));

	// TEST_REPORT("Jitter with idle", 260);
	// for (int i = 0; i < MAX_JITTER_TASKS; i++) {
	// 	printf("Jitter task %d:\n", i);
	// 	for (int j = 0; j < JITTER_SAMPLES; j++) {
	// 		printf("%llu%c", common.jitter[i][j], (j == JITTER_SAMPLES - 1) ? '\n' : ',');
	// 	}
	// }

	// for (int i = 0; i < MAX_JITTER_TASKS; i++) {
	// 	resourceDestroy(common.jitterCond[i]);
	// 	resourceDestroy(common.jitterMutex[i]);
	// }
}
