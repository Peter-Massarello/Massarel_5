#define _XOPEN_SOURCE 700

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <signal.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/sem.h>

#define MAX 18
#define MAX_RESOURCE 20
#define INSTANCE 10
#define SEMS 2
#define RES_SEM 1
#define CLOCK_SEM 1

#define NOT 0
#define NORMAL 1
#define EARLY 2

void cleanup();
void sig_handler();
void get_shm();
void get_sem();
void log_message(char*);
void sem_wait(int);
void sem_signal(int);
void child_handler(int);
void get_shm();
void get_sem();
void init_shm();
void init_sem();
void fork_next();
bool time_to_fork();
void sem_signal(int);
void sem_wait(int);
void normalize_shm_clock();
void normalize_fork_clock();
void increment_shm_clock();
void finish_check();
void release_handler();
void allocation_handler();
void deadlock_check();
void print_allocations();
void kill_pids();


typedef struct {
	bool shareable;
	bool taken;

	int instances;
	int instances_remaining;

	int request[MAX];
	int allocated[MAX];
	int release[MAX];
	int max[MAX];
} resource_t;

typedef struct {
	int deadlock_count;

	int pids[MAX];

	unsigned int cpu_time[MAX];
	unsigned int wait_time[MAX];

	unsigned int clock_nano;
	unsigned int clock_seconds;
	unsigned int clock_ms;

	bool finished[MAX];
	bool sleep[MAX];
	bool waiting[MAX];

	int wants[MAX];

	resource_t res[MAX_RESOURCE];

} shmptr_t; 
