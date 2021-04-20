#include "oss.h"

int shmid;
int semid;
shmptr_t* shm_ptr;

int index, my_pid;
unsigned long wait_for_ns;
unsigned long wait_for_sec;
unsigned long rand_time, rand_res, rand_inst;

int get_index(int);
bool check_time();

int main() {

	int resources_used, temp;

	my_pid = getpid();
	srand(my_pid * 12);

	signal(SIGKILL, sig_handler);
	signal(SIGALRM, sig_handler);
	signal(SIGINT, sig_handler);

	get_shm();
	//log_message("shared memory created\n");
	get_sem();
	//log_message("semaphores created\n");

	index = get_index(my_pid);

	shm_ptr->wants[index] = -1;
	shm_ptr->waiting[index] = false;
	shm_ptr->finished[index] = NOT;
	shm_ptr->sleep[index] = false;

	for (int i = 0; i < MAX_RESOURCE; i++) {
		shm_ptr->res[i].allocated[index] = 0;
		shm_ptr->res[i].request[index] = 0;
		shm_ptr->res[i].release[index] = 0;
	}

	sem_wait(CLOCK_SEM);

	unsigned long start_ns = shm_ptr->clock_nano;
	unsigned long start_sec = shm_ptr->clock_seconds;

	sem_signal(CLOCK_SEM);

	wait_for_ns = start_ns;
	wait_for_sec = start_sec;

	while (shm_ptr->finished[index] != NORMAL) {
		while (1) {
			if (check_time()) {
				break;
			}
			if (shm_ptr->sleep[index] == true || shm_ptr->waiting[index] == true) {

			}
			else {
				if (shm_ptr->clock_seconds - start_sec > 1) {
					if (shm_ptr->clock_nano > start_ns) {
						if (rand() % 10 == 1) {
							sem_wait(RES_SEM);

							shm_ptr->finished[index] = EARLY;

							sem_signal(RES_SEM);
							cleanup();
							exit(0);
							break;
						}
					}
				}
				rand_res = rand() % MAX_RESOURCE;

				sem_wait(RES_SEM);

				if (shm_ptr->res[rand_res].allocated[index] > 0) {
					if (rand() % 10 < 5) {
						shm_ptr->res[rand_res].release[index] = shm_ptr->res[rand_res].allocated[index];
						shm_ptr->waiting[index] = true;
					}
				}
				else {
					if (rand() % 10 < 5) {
						if (rand() % 10 < 5) {
							rand_inst = 1 + (rand() % shm_ptr->res[rand_res].instances);
							if (rand_inst > 0) {
								shm_ptr->res[rand_res].request[index] = rand_inst;
								shm_ptr->waiting[index] = true;
							}
						}
					}
				}
				sem_signal(RES_SEM);

				sem_wait(CLOCK_SEM);

				rand_time = rand() % 250000000;
				wait_for_ns = shm_ptr->clock_nano;
				wait_for_ns += rand_time;
				if (wait_for_ns >= 1000000000) {
					wait_for_ns -= 1000000000;
					wait_for_sec++;
				}

				sem_signal(CLOCK_SEM);
			}
		}
		cleanup();

		return 0;
	}

	rand_time = rand() % 250000000;
	wait_for_ns += rand_time;
	if (wait_for_ns >= 1000000000) {
		wait_for_ns -= 1000000000;
		wait_for_sec++;
	}

	return 0;
}

bool check_time() {
	if (shm_ptr->clock_seconds > wait_for_sec) {
		return true;
	}
	else if (shm_ptr->clock_seconds == wait_for_sec) {
		if (shm_ptr->clock_nano > wait_for_ns) {
			return true;
		}
	}
	return false;
}

int get_index(int pid) {
	for (int i = 0; i < MAX; i++) {
		if (pid == shm_ptr->pids[i]) {
			return i;
		}
	}
	return -1;
}

void sig_handler() {
	cleanup();
	exit(0);
}

void sem_signal(int sem_id) {
	struct sembuf op;
	op.sem_num = 0;
	op.sem_op = 1;
	op.sem_flg = 0;
	semop(sem_id, &op, 1);
}

void sem_wait(int sem_id) {
	struct sembuf op;
	op.sem_num = 0;
	op.sem_op = -1;
	op.sem_flg = 0;
	semop(sem_id, &op, 1);
}

void cleanup() {
	shmdt(shm_ptr);
}

void get_shm() {
	//printf("In get_shm()\n");
	key_t key = ftok("Makefile", 'a');
	if ((shmid = shmget(key, (sizeof(resource_t) * MAX) + sizeof(shmptr_t), IPC_CREAT | 0666)) == -1) {
		perror("oss.c: shmget failed:");
		exit(0);
	}

	if ((shm_ptr = (shmptr_t*)shmat(shmid, 0, 0)) == (shmptr_t*)-1) {
		perror("oss.c: shmat failed:");
		exit(0);
	}
}

void get_sem() {
	key_t key = ftok(".", 'a');

	if ((semid = semget(key, SEMS, IPC_CREAT | 0666)) == -1) {
		errno = 5;
		perror("oss.c: Error: Semaphores could not be created in get_sem()");
		cleanup();
		exit(0);
	}
}