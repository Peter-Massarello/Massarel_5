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


typedef struct {

	int user_count;
	int deadlock_count;


	int scheduled_pid;
	int scheduled_index;

	unsigned int next_fork_sec;
	unsigned int next_fork_nano;
	unsigned int clock_nano;
	unsigned int clock_seconds;


	pcb_t pcb_arr[MAX];

} shmptr_t;