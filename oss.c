#include "oss.h"

int shmid, semid;
int log_count = 0;
int fork_count = 0;
int num_proc = 0;
int immediate = 0;
int deadlock_kill = 0;
int after_waiting = 0;
int detections = 0;

char buffer[200];


FILE* fp;
shmptr_t* shm_ptr;
extern int errno;

unsigned long next_fork_sec = 0;
unsigned long next_fork_nano = 0;



int main(){
	signal(SIGKILL, sig_handler);
	signal(SIGALRM, sig_handler);
	signal(SIGINT, sig_handler);

	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = child_handler;
	sigaction(SIGCHLD, &sa, NULL);

	fp = fopen("logfile.log", "a");

	int proc_count = 0;
	

	get_shm();
	//log_message("shared memory created\n");
	get_sem();
	//log_message("semaphores created\n");

	init_shm();
	//log_message("shared memory init\n");
	init_sem();
	//log_message("semaphores init\n");

	alarm(5);

	while (1) {
		if (num_proc == 0) {
			next_fork_nano = rand() % 5000000;
			printf("%d\n", num_proc);
			fork_next();
		}
		else if (num_proc < MAX) {
			if (time_to_fork()) {
				fork_next();
				log_message("Generateing fork time");
				next_fork_nano = shm_ptr->clock_nano + (rand() % 50000000);
				normalize_fork_clock();
			}
		}

		sem_wait(RES_SEM);

		if (num_proc > 0) {
			finish_check();
			release_handler();
			allocation_handler();
			deadlock_check();
			detections++;
			print_allocations();
		}

		sem_signal(RES_SEM);


		sem_wait(CLOCK_SEM);

		increment_shm_clock();

		sem_signal(CLOCK_SEM);
	}


	return 0;
}

void increment_shm_clock() {
	unsigned long nano = 1 + (rand() % 100000000);
	shm_ptr->clock_nano += nano;
	shm_ptr->clock_ms += nano / 1000000;
	normalize_shm_clock();
}

void normalize_fork_clock() {
	unsigned long nano = next_fork_nano;
	if (nano >= 1000000000) {
		next_fork_sec += 1;
		next_fork_nano -= 1000000000;
	}
}

void normalize_shm_clock() {
	unsigned long nano = shm_ptr->clock_nano;
	if (nano >= 1000000000) {
		shm_ptr->clock_seconds += 1;
		shm_ptr->clock_nano -= 1000000000;
	}
}

void print_allocations() {
	fputs("\n\tResource allocation chart\n", fp);
	fputs("    P0  P1  P2  P3  P4  P5  P6  P7  P8  P9  P10  P11  P12  P13  P14  P15  P16  P17  P18  P19\n", fp);
	for (int i = 0; i < MAX_RESOURCE; i++) {
		sprintf(buffer, "R%d   ", i);
		fputs(buffer, fp);
		for (int j = 0; j < MAX; j++) {
			sprintf(buffer, "%d    ", shm_ptr->res[i].allocated[j]);
			fputs(buffer, fp);
		}
		fputs("\n", fp);
	}

}

void allocation_handler() {
	for (int i = 0; i < MAX; i++) {
		if (shm_ptr->sleep[i] == false) {
			for (int j = 0; j < MAX_RESOURCE; j++) {
				if (shm_ptr->res[j].request[i] > 0) {
					sprintf(buffer, "Master has detected a process P%d that wants access to R%d\n", i, j);
					log_message(buffer);

					if (shm_ptr->res[j].request[i] <= shm_ptr->res[j].instances_remaining) {
						shm_ptr->res[j].instances_remaining -= shm_ptr->res[j].request[i];
						shm_ptr->res[j].allocated[i] = shm_ptr->res[j].request[i];
						shm_ptr->res[j].request[i] = 0;
						shm_ptr->sleep[i] = false;
						shm_ptr->waiting[i] = false;
						immediate++;

						sprintf(buffer, "Master has detected process P%d recieving access to R%d at time %d : %d\n", i, j, shm_ptr->clock_seconds, shm_ptr->clock_nano);
						log_message(buffer);
					}
					else {
						sprintf(buffer, "Master has detected process P%d going to sleep at time %d : %d\n", i, shm_ptr->clock_seconds, shm_ptr->clock_nano);
						log_message(buffer);
						shm_ptr->wants[i] = j;
						shm_ptr->sleep[i] = true;
					}
				}
			}
		}
	}
}

void deadlock_check() {
	for (int i = 0; i < MAX; i++) {
		if (shm_ptr->sleep[i] == true) {
			int res = shm_ptr->wants[i];
			while (1) {
				for (int j = 0; j < MAX_RESOURCE; j++) {
					if (j != i) {
						if (shm_ptr->res[res].allocated[j] > 0 && shm_ptr->sleep[j] == true) {
							sprintf(buffer, "Master has detected a deadlock at P%d and will be killed to free R%d for P%d\n", j, res, i);
							log_message(buffer);
							deadlock_kill++;

							shm_ptr->res[res].instances_remaining += shm_ptr->res[res].allocated[j];

							shm_ptr->res[res].release[j] = 0;
							shm_ptr->res[res].request[j] = 0;
							shm_ptr->res[res].allocated[j] = 0;

							kill(shm_ptr->pids[j], SIGKILL);

							shm_ptr->pids[j] = 0;

							if (shm_ptr->res[res].request[i] <= shm_ptr->res[res].instances_remaining) {
								shm_ptr->res[res].instances_remaining -= shm_ptr->res[res].request[i];
								shm_ptr->res[res].request[i] = 0;

								sprintf(buffer, "Master has deteced process P%d getting access to R%d at time %d : %d\n", i, res, shm_ptr->clock_seconds, shm_ptr->clock_nano);
								log_message(buffer);

								after_waiting++;

								shm_ptr->sleep[i] = false;
								shm_ptr->waiting[i] = false;
								break;
							}

						}
					}
				}
			}
		}
	}
}

void finish_check() {
	for (int i = 0; i < MAX; i++) {
		if (shm_ptr->finished[i] == EARLY) {
			sprintf(buffer, "Master has detected that process P%d terminated early at time %d : %d, releasing resources\n", i, shm_ptr->clock_seconds, shm_ptr->clock_nano);
			log_message(buffer);

			for (int j = 0; j < MAX_RESOURCE; j++) {
				if (shm_ptr->res[j].allocated[i] > 0) {
					shm_ptr->res[j].instances_remaining += shm_ptr->res[j].allocated[i];

					shm_ptr->res[j].allocated[i] = 0;
					shm_ptr->res[j].release[i] = 0;
					shm_ptr->res[j].request[i] = 0;

					shm_ptr->pids[i] = 0;
				}
			}
		}
	}
}

void release_handler() {
	for (int i = 0; i < MAX; i++) {
		for (int j = 0; i < MAX; i++) {
			if (shm_ptr->res[j].release[i] > 0) {
				sprintf(buffer, "Master has detected process P%d resource R%d being released at time %d : %d \n", i, j, shm_ptr->clock_seconds, shm_ptr->clock_nano);
				log_message(buffer);

				shm_ptr->res[j].instances_remaining += shm_ptr->res[j].allocated[i];
				shm_ptr->res[j].release[i] = 0;
				shm_ptr->waiting[i] = false;
			}
		}
	}
}

void fork_next() {
	fork_count++;
	if (fork_count >= 40) {
		printf("%d", fork_count);
		printf("Program has reached its maximum number of forks, exiting...\n");
		cleanup();
		exit(0);
	}
	for (int i = 0; i < MAX; i++) {
		if (shm_ptr->pids[i] == 0) {
			num_proc++;
			int pid = fork();

			if (pid != 0) {
				sprintf(buffer, "Master has detected process P%d being forked at time %d : %d \n", i, shm_ptr->clock_seconds, shm_ptr->clock_nano);
				log_message(buffer);
				shm_ptr->pids[i] = pid;
				return;
			}
			else {
				execl("./uproc", "./uproc", (char*)0);
			}
		}
	}
}

bool time_to_fork() {
	if (next_fork_sec == shm_ptr->clock_seconds) 
		if (next_fork_nano <= shm_ptr->clock_nano) 
			return true;
	if (next_fork_sec > shm_ptr->clock_seconds)
		return true;

	return false;
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

void kill_pids() {
	for (int i = 0; i < MAX; i++) {
		if (shm_ptr->pids[i] != 0) {
			kill(shm_ptr->pids[i], SIGKILL);
		}
	}
}

void cleanup() {
	//print_allocations();
	fclose(fp);
	kill_pids();
	sleep(1);
	shmdt(shm_ptr);
	shmctl(shmid, IPC_RMID, NULL);
	semctl(semid, 0, IPC_RMID, NULL);
}

void sig_handler() {
	cleanup();
	exit(0);
}

void init_shm() {
	for (int i = 0; i < MAX_RESOURCE; i++) {
		srand(time(NULL));
		int shared_rand = (rand() % 20) + 1; // Random numbers from 1-20

		if (shared_rand <= 4) // if less that 4 (about 20% of the time) make it shareable
			shm_ptr->res[i].shareable = true;
		else
			shm_ptr->res[i].shareable = false;

		shm_ptr->res[i].taken = false;
		shm_ptr->res[i].instances = 1 + (rand() % MAX);
		shm_ptr->res[i].instances_remaining = shm_ptr->res[i].instances;

		for (int j = 0; j < MAX; j++) {
			shm_ptr->res[i].request[j] = 0;
			shm_ptr->res[i].allocated[j] = 0;
			shm_ptr->res[i].release[j] = 0;
			shm_ptr->res[i].max[j] = 0;
		}
	}

	for (int i = 0; i < MAX; i++) {
		shm_ptr->pids[i] = 0;
		shm_ptr->cpu_time[i] = 0;
		shm_ptr->wait_time[i] = 0;
		shm_ptr->sleep[i] = false;
		shm_ptr->finished[i] = false;
		shm_ptr->waiting[i] = false;
	}

	shm_ptr->clock_seconds = 0;
	shm_ptr->clock_nano = 0;
	shm_ptr->clock_ms = 0;
}

void init_sem() {
	semctl(semid, RES_SEM, SETVAL, 1);
	semctl(semid, CLOCK_SEM, SETVAL, 1);
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

void child_handler(int sig) {
	pid_t pid;
	while ((pid = waitpid((pid_t)(-1), 0, WNOHANG)) > 0) {
		// Do stuff with children
	}
}

void log_message(char* message) {
	log_count++;
	if (log_count <= 100000) {
		fputs(message, fp);
	}
	else {
		printf("Logfile reached maximum number of lines, exiting...\n");
		cleanup();
		exit(0);
	}
}
