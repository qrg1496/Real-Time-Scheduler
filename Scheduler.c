/*Scheduling Project for Performance Engineering of Real-Time and Embedded Systems
 *
 * Authors: Quentin Goyert,
 */

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>
#include <sys/trace.h>
#include <sys/neutrino.h>
#include <time.h>
#include <sys/netmgr.h>


#define STATE_IDLE 0;
#define STATE_READY 1;
#define STATE_RUNNING 2;

static pthread_t scheduleThreadID;
pthread_mutex_t mutex;

typedef struct{
		int runTime;
		int period;
		int deadline; // Date time that it needs to be done by.
		pthread_t threadID;
		pthread_mutex_t mutex;
		int state;
} ProgramInfo;

//ProgramInfo * programsArray;
static int numPrograms = 1;
pthread_cond_t control;

int pid;
int chid;
struct _pulse pulse;
int pulse_id = 0;


void * program(void * arg){
	//int location = (int)&arg[0];
	ProgramInfo * program = (ProgramInfo *)arg;
	int time = program->runTime;

	while(1){
		pthread_mutex_lock(&(program->mutex));
		program->state = STATE_RUNNING;
		nanospin_ns(time * 1000000);

		program->state = STATE_IDLE;

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

	ProgramInfo buffer;
	int back;
	for(loopCounter = 1; loopCounter < numPrograms; loopCounter++){
		buffer= programsArray[loopCounter];
		back = loopCounter-1;
		while(back >= 0 && (programsArray[loopCounter].period < buffer.period)){
			programsArray[back+1]=programsArray[back];
			back--;
		}
		programsArray[back+1]=buffer;

	}

	for(loopCounter = 0; loopCounter < numPrograms; loopCounter++){
		progParameters.sched_priority--;

		pthread_attr_setschedparam(&progThreadAttributes, &progParameters);
		pthread_create(&(programsArray[loopCounter].threadID), &progThreadAttributes, &program, (void *)(programsArray + loopCounter));
	}

	for(;;){
			pid= MsgReceivePulse(chid, &pulse, sizeof(pulse),NULL);
			for(loopCounter = 1; loopCounter < numPrograms; loopCounter++){
				if(programsArray[loopCounter].deadline != 0 && programsArray[loopCounter].state == 2)
					{
					programsArray[loopCounter].deadline--;
					}
				if(programsArray[loopCounter].deadline == 0){
					if(programsArray[loopCounter].state == 0){
						programsArray[loopCounter].deadline=programsArray[loopCounter].period;
						programsArray[loopCounter].state = 1;
						pthread_mutex_unlock(&(programsArray[loopCounter].mutex));
					}
					else{
						perror("Program Failure Detected\n");
						printf("Program %i Failed to meet Deadline\n",loopCounter);
						exit(0);
					}
				}
			}

		}

}

/*void * earliestDeadlineScheduler(void * arg){

}

void * leastSlackTime(void * arg){

}*/

ProgramInfo* ReadData(){
	char buffer[11];
	printf("Number of Programs?\n");
	fgets(buffer,sizeof(buffer),stdin);
	numPrograms = atoi(buffer);
	ProgramInfo programArray[numPrograms];
	int i;
	for(i = 0; i < numPrograms; i++){
		printf("Runtime of Program %i?\n", i);
		fgets(buffer,sizeof(buffer),stdin);
		programArray[i].runTime=atoi(buffer);

		printf("Period of Program %i?\n",i);
		fgets(buffer,sizeof(buffer),stdin);
		programArray[i].period=atoi(buffer);

		printf("Deadline of Program %i?\n",i);
		fgets(buffer,sizeof(buffer),stdin);
		programArray[i].deadline=atoi(buffer);

		if(programArray[i].period != programArray[i].deadline){
			perror("Deadline Not Equal to Period!\n");
			exit(EXIT_FAILURE);
		}

		programArray[i].state=STATE_READY;
		pthread_mutex_init(&(programArray[i].mutex),NULL);
	}

	return programArray;
}

int main(int argc, char *argv[]) {

	pthread_attr_t threadAttributes;
	struct sched_param parameters;
	int policy;
	pthread_attr_init(&threadAttributes);
	pthread_getschedparam(pthread_self(),&policy, &parameters);
	struct _clockperiod clkper;
	struct sigevent event;
	struct itimerspec timer;
	timer_t timer_id;



	parameters.sched_priority--;

	pthread_attr_setschedparam(&threadAttributes, &parameters);

	pthread_mutex_init(&mutex,NULL);
	nanospin_calibrate(0);

	/*Real Time Clock Setup*/
	clkper.nsec = 10000000;
	clkper.fract = 0;

	ClockPeriod(CLOCK_REALTIME, &clkper, NULL, 0);

	chid = ChannelCreate(0);
	assert(chid != -1);

	/*Event creation and Set up*/
	event.sigev_notify = SIGEV_PULSE;
	event.sigev_coid = ConnectAttach(ND_LOCAL_NODE,0,chid,0,0);
	event.sigev_priority = getprio(0);
	event.sigev_code = 1023;
	event.sigev_value.sival_ptr = (void *)pulse_id;

	/*Timer Set up and Creation*/
	timer_create( CLOCK_REALTIME, &event, &timer_id );

	timer.it_value.tv_sec=0;
	timer.it_value.tv_nsec=10000000;
	timer.it_interval.tv_sec=0;
	timer.it_interval.tv_nsec=10000000;

	timer_settime( timer_id, 0, &timer, NULL );

	ProgramInfo * programArray = ReadData();

	pthread_cond_init(&control, NULL);



	pthread_create(&scheduleThreadID,&threadAttributes, &rateMonotonicScheduler, (void *)programArray);




	return EXIT_SUCCESS;

}


