#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern void my_malloc_stats(void);
extern void *my_malloc(size_t size);
extern void my_free(void *ptr);
extern void *my_calloc(size_t nmemb, size_t size);

static void *thread_1(void *m2) {
	void *m1;
	m1 = my_malloc(64);
	memset(m1, 'E', 64);
	sleep(2);
	my_free(m1);
	my_free(m2);
	m1 = my_calloc(2, 32);
	memset(m1, 'F', 10);
	return NULL;
}

static void *thread_2(void *arg __attribute__((unused))) {
	void *m1;
	sleep(1);
	m1 = my_malloc(96);
	memset(m1, 'G', 70);
	return NULL;
}

static void fault_handler(int signum, siginfo_t *siginfo __attribute__((unused)), void *context __attribute__((unused))) {
	printf("Caught signal %d: %s!\n", signum, strsignal(signum));
}

void hw4_test(void) {
	void *m1, *m2;
	char *c1;

	my_malloc_stats();

	m1 = my_malloc(30);
	memset(m1, 'A', 30);
	m2 = my_malloc(50);
	memset(m2, 'B', 33);
	my_free(m1);
	m1 = my_malloc(15);
	memset(m1, 'C', 8);
	my_free(m1);
	my_malloc_stats();

	errno = 0;
	m1 = my_malloc(161);
	if (m1 == NULL) {
		printf("Out of memory! Reason: %s\n", strerror(errno));
	}
	m1 = my_calloc(2, 16);
	c1 = (char *) m1;
	if (c1[0] == 0 && c1[15] == 0) {
		printf("Good, memory was zeroed.\n");
	}
	memset(m1, 'D', 16);

	my_malloc_stats();
	
	pthread_t pth[2];
	if (pthread_create(pth + 0, NULL, thread_1, m2) != 0 ||
	    pthread_create(pth + 1, NULL, thread_2, NULL) != 0) {
		exit(EXIT_FAILURE);
	}
	if (pthread_join(pth[0], NULL) != 0 ||
	    pthread_join(pth[1], NULL) != 0) {
		exit(EXIT_FAILURE);
	}


	my_malloc_stats();

	struct sigaction sa = {
		.sa_sigaction = fault_handler,
		.sa_flags = SA_SIGINFO,
	};
	sigemptyset(&sa.sa_mask);
	if (sigaction(SIGSEGV, &sa, NULL) < 0) {
		exit(EXIT_FAILURE);
	}
	my_free(m1);
	my_free(m1);
}
