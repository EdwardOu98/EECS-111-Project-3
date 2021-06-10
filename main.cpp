#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <sys/time.h>
#include <string>
#include <vector>
#include <unistd.h>
#include <pthread.h>
#include "types_p3.h"
#include "p3_threads.h"
#include "utils.h"

pthread_cond_t  cond[4];
pthread_cond_t a_task_is_done;
ThreadCtrlBlk   tcb[4];
std::vector<int> readyQue;

int num_of_alive_tasks=4;
int occupied = 0;


pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t taskDoneMutex = PTHREAD_MUTEX_INITIALIZER;

struct timeval t_global_start;
int global_work = 0;

void fifo_schedule(void);
void edf_schedule(void);
void rm_schedule(void);
int edf_idx(void);
int rm_idx(void);

int main(int argc, char** argv)
{
	if(argc !=2 || atoi(argv[1]) < 0 || atoi(argv[1]) > 2)
	{
		std::cout << "[ERROR] Expecting 1 argument, but got " << argc-1 << std::endl;
		std::cout<< "[USAGE] p3_exec <0, 1, or 2>" << std::endl;
		return 0;
	}
	int schedule = atoi(argv[1]);



	pthread_t       tid[4];
	int             status = 0;
	int             id = 0;
	long            stamp = 0;
	int             mode = 0;


	
	// This is to set the global start time
	gettimeofday(&t_global_start, NULL);	



	tcb[0].id = 0;
	tcb[0].task_time = 200;
	tcb[0].period = 1000;
	tcb[0].deadline = 1000;

	tcb[1].id = 1;
	tcb[1].task_time = 500;
	tcb[1].period = 2000;
	tcb[1].deadline = 2000;

	tcb[2].id = 2;
	tcb[2].task_time = 1000;
	tcb[2].period = 4000;
	tcb[2].deadline = 4000;

	tcb[3].id = 3;
	tcb[3].task_time = 1000;
	tcb[3].period = 6000;
	tcb[3].deadline = 6000;

	for (int i=0; i<4; i++) {
		pthread_cond_init (&(cond[i]), NULL); 
	}
	pthread_cond_init (&a_task_is_done, NULL); 
	

	global_work = 1;
	printf("[Main] Create worker threads\n");
	for (int i=0; i<4; i++) {
		if(pthread_create(&(tid[i]), NULL, threadfunc, &(tcb[i]))) {
			fprintf(stderr, "Error creating thread\n");		
		}
	}

	// Wait until the thread is in place
	usleep(MSEC(1000));
	
	
	// This is to reset the global time and skip the initial wait
	gettimeofday(&t_global_start, NULL);	
	
	
	int sleep = 0;
	for (int i=0; i<240; i++) {	
		stamp = get_time_stamp();

		/////////////////////////////////////////////////////////
		// Implement different scheduling algorithms
        // Select different schedule based on mode value
		
		switch(schedule)
		{
			case 0:
				fifo_schedule();
				break;
			case 1:
				edf_schedule();
				break;
			case 2:
				rm_schedule();
				break;
		}
		/////////////////////////////////////////////////////////
		// Wait for a fraction of 100ms or until a task is done.

		stamp = get_time_stamp();
		sleep = 100 - (stamp%100);
		if (num_of_alive_tasks>0)
			timed_wait_for_task_complition(sleep);
		else{
			printf("All the tasks missed the deadline");
			break;
		}
	}


	printf("[Main] It's time to finish the thread\n");



	printf("[Main] Locks\n");
	pthread_mutex_lock(&mutex);
	global_work = 0;
	printf("[Main] Unlocks\n");

	
	// make sure all the process are in the ready queue
	usleep(MSEC(3000));
	while (readyQue.size()>0) {
		pthread_cond_signal(&(cond[readyQue[0]]));
		readyQue.erase(readyQue.begin()+0);

	}


	pthread_mutex_unlock(&mutex);



	/* wait for the second thread to finish */
	for (int i=0; i<4; i++) {
		if(pthread_join(tid[i], NULL)) {
			fprintf(stderr, "Error joining thread\n");	
		}
	}


	return 0;
}

// Find the thread that has the earliest deadline
int edf_idx(void){
	int idx = 0;
	int ed = tcb[readyQue[0]].deadline; // earliest deadline
	// printf("Thread %d has deadline %d.\n", readyQue[0], tcb[readyQue[0]].deadline);
	for(int i = 1; i < readyQue.size(); i++){
		// printf("Thread %d has deadline %d.\n", readyQue[i], tcb[readyQue[i]].deadline);
		if(tcb[readyQue[i]].deadline < ed){
			idx = i;
			ed = tcb[readyQue[i]].deadline;
		}
	}
	return idx;
}

// Find the thread that has the shortest period
int rm_idx(void){
	int idx = 0;
	int sp = tcb[readyQue[0]].period; // shortest period
	// printf("Thread %d has period %d.\n", readyQue[0], tcb[readyQue[0]].period);
	for(int i = 1; i < readyQue.size(); i++){
		// printf("Thread %d has period %d.\n", readyQue[i], tcb[readyQue[i]].period);
		if(tcb[readyQue[i]].period < sp){
			idx = i;
			sp = tcb[readyQue[i]].period;
		}
	}
	return idx;
}

void fifo_schedule(void)
{
	pthread_mutex_lock(&mutex);
	if((!readyQue.empty()) && (!occupied)){
		pthread_cond_signal(&(cond[readyQue[0]]));
		readyQue.erase(readyQue.begin());
	}
	pthread_mutex_unlock(&mutex);
}


void edf_schedule(void)
{
	pthread_mutex_lock(&mutex);
	int idx;
	if((!readyQue.empty()) && (!occupied)){
		idx = edf_idx();
		pthread_cond_signal(&(cond[readyQue[idx]]));
		readyQue.erase(readyQue.begin() + idx);
	}
	pthread_mutex_unlock(&mutex);
}


void rm_schedule(void)
{
	pthread_mutex_lock(&mutex);
	int idx;
	if((!readyQue.empty()) && (!occupied)){
		idx = rm_idx();
		pthread_cond_signal(&(cond[readyQue[idx]]));
		readyQue.erase(readyQue.begin() + idx);
	}
	pthread_mutex_unlock(&mutex);
}

