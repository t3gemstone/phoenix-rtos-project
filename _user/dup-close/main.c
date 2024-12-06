#include <stdio.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/threads.h>
#include <sys/file.h>
#include <sys/time.h>
#include <fcntl.h>


#define NUM_THREADS            100
#define THREAD_STACK_SIZE      4096
#define BENCHMARK_DURATION_SEC 15ULL


typedef struct {
	int thread_id;
	int fd;
} thread_arg_t;

atomic_int taskStart = 0;
unsigned int thread_ops_count[NUM_THREADS] = { 0 };

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


void benchmark_thread(void *arg)
{
	thread_arg_t *thread_arg = (thread_arg_t *)arg;
	int fd = thread_arg->fd;
	int dup_fd;

	while (!taskStart) {
		usleep(0);
	}

	uint64_t start_cycles = getCntr();
	uint64_t end_cycles = start_cycles + (BENCHMARK_DURATION_SEC * 250000000ULL);  // 250 MHz * 1 second

	while (getCntr() < end_cycles) {
		dup_fd = dup(fd);
		if (dup_fd < 0) {
			perror("dup");
			endthread();
		}
		close(dup_fd);
		thread_ops_count[thread_arg->thread_id]++;
	}

	endthread();
}


int main(int argc, char *argv[])
{
	static unsigned char stacks[NUM_THREADS][THREAD_STACK_SIZE];
	static handle_t threads[NUM_THREADS];
	static thread_arg_t thread_args[NUM_THREADS];

	priority(0);

	int nthreads = NUM_THREADS;

	if (argc > 1) {
		nthreads = atoi(argv[1]);
		if (nthreads > NUM_THREADS) {
			nthreads = NUM_THREADS;
			printf("Number of threads limited to %d\n", NUM_THREADS);
		}
	}

	printf("Starting benchmark with %d threads for %llu seconds\n", nthreads, BENCHMARK_DURATION_SEC);
	int fd = open("/dev/console", O_RDWR);
	for (int i = 0; i < nthreads; i++) {
		if (fd < 0) {
			perror("open");
			return -1;
		}

		thread_args[i].thread_id = i;
		thread_args[i].fd = fd;

		if (beginthreadex(benchmark_thread, 2, stacks[i], THREAD_STACK_SIZE, &thread_args[i], &threads[i]) < 0) {
			perror("beginthreadex");
			return -1;
		}
	}

	priority(4);
	taskStart = 1;

	for (int i = 0; i < nthreads; i++) {
		threadJoin(threads[i], 0);
	}
	close(fd);

	uint64_t ops_count = 0;
	for (int i = 0; i < nthreads; i++) {
		ops_count += thread_ops_count[i];
		printf("Thread %d operations: %u\n", i, thread_ops_count[i]);
	}

	printf("Benchmark completed\n");
	printf("Total operations: %llu\n", ops_count);
	printf("Operations per second: %llu\n", ops_count / BENCHMARK_DURATION_SEC);

	return 0;
}
