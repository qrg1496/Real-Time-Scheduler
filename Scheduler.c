/*Scheduling Project for Performance Engineering of Real-Time and Embedded Systems
 *
 * Authors: Quentin Goyert,
 */

#include <stdlib.h>
#include <stdio.h>

#include <pthread.h>
#include <sys/trace.h>
#include <sys/neutrino.h>
#include <time.h>

#define STATE_IDLE 0;
#define STATE_READY 1;
#define STATE_RUNNING 2;

static pthread_t scheduleThreadID;
pthread_mutex_t mutex;

typedef struct{
		int runTime;
		int period;
		long deadline; // Date time that it needs to be done by.
		pthread_t threadID;
		int state;
} ProgramInfo;

//ProgramInfo * programsArray;
static int numPrograms = 1;
pthread_cond_t control;


void * program(void * arg){
	//int location = (int)&arg[0];
	ProgramInfo * program = (ProgramInfo *)arg;
	int time = program->runTime;

	while(1){
		program->state = STATE_RUNNING;
		nanospin_ns(time * 1000000);

		pthread_mutex_lock(&mutex);
		pthread_cond_broadcast(&control);
		program->state = STATE_IDLE;
		pthread_mutex_unlock(&mutex);
	}
}

void * rateMonotonicScheduler(void * arg){

	//int progNumbers = (int)&arg[0];
	ProgramInfo * programsArray = (ProgramInfo *)arg;
	pthread_attr_t progThreadAttributes;
	struct sched_param progParameters;
	int policy;
	int loopCounter;

	pthread_attr_init(&progThreadAttributes);
	pthread_getschedparam(pthread_self(),&policy, &progParameters);

	for(loopCounter = 0; loopCounter < numPrograms; loopCounter++){
		progParameters.sched_priority--;

		pthread_attr_setschedparam(&progThreadAttributes, &progParameters);
		pthread_create(&(programsArray[loopCounter].threadID), &progThreadAttributes, &program, (void *)(programsArray + loopCounter));
	}

	while(1){
		pthread_mutex_lock(&mutex);
		pthread_cond_wait(&control,&mutex);
		pthread_mutex_unlock(&mutex);
		//Check state and deadlines of all threads.

		for(loopCounter = 0; loopCounter < numPrograms; loopCounter++){
			ProgramInfo program = programsArray[loopCounter];


		}

	}

}

/*void * earliestDeadlineScheduler(void * arg){

}

void * leastSlackTime(void * arg){

}*/

int main(int argc, char *argv[]) {
	printf("Welcome to the QNX Momentics IDE\n");

	pthread_attr_t threadAttributes;
	struct sched_param parameters;
	int policy;
	pthread_attr_init(&threadAttributes);
	pthread_getschedparam(pthread_self(),&policy, &parameters);

	parameters.sched_priority--;

	pthread_attr_setschedparam(&threadAttributes, &parameters);

	pthread_mutex_init(&mutex,NULL);
	nanospin_calibrate(0);

	pthread_cond_init(&control, NULL);

	pthread_create(&scheduleThreadID,&threadAttributes, &rateMonotonicScheduler, 0);


	return EXIT_SUCCESS;

}


