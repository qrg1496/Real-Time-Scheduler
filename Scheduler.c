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


#define STATE_IDLE 0
#define STATE_READY 1
#define STATE_RUNNING 2

static pthread_t scheduleThreadID;
pthread_mutex_t mutex;

typedef struct{
		int runTime;
		int period;
		int deadline; // Date time that it needs to be done by.
		pthread_t threadID;
		pthread_mutex_t mutex;
		int state;
		//int slackTime; //Time to deadline when program complete
} ProgramInfo;

ProgramInfo * programsArray;
static int numPrograms = 0;
pthread_cond_t control;

int pid;
int chid;
struct _pulse pulse;
int pulse_id = 0;


void * program(void * arg){
	int location = (int)&arg[0];
	//ProgramInfo * program = (ProgramInfo *)arg;
	int time = programsArray[location].runTime;

	while(1){
		//pthread_mutex_lock(&(programsArray[location].mutex));
		if(programsArray[location].state == STATE_READY){
			programsArray[location].state = STATE_RUNNING;
			nanospin_ns(time * 1000000);

			//programsArray[location].slackTime = programsArray[location].deadline;
			programsArray[location].state = STATE_IDLE;
		}

	}
}

void * rateMonotonicScheduler(void * arg){

	//int progNumbers = (int)&arg[0];
	//ProgramInfo * programsArray = (ProgramInfo *)arg;
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
		pthread_create(&(programsArray[loopCounter].threadID), &progThreadAttributes, &program, (void *)loopCounter);
	}

	for(;;){
			pid= MsgReceivePulse(chid, &pulse, sizeof(pulse),NULL);
			for(loopCounter = 0; loopCounter < numPrograms; loopCounter++){
				if(programsArray[loopCounter].deadline != 0)
					{
					programsArray[loopCounter].deadline--;
					}
				if(programsArray[loopCounter].deadline == 0){
					if(programsArray[loopCounter].state == STATE_RUNNING || programsArray[loopCounter].state == STATE_READY){
						perror("Program Failure Detected\n");
						printf("Program %i Failed to meet Deadline\n",loopCounter);
						exit(0);
					}
					if(programsArray[loopCounter].state == STATE_IDLE){
						programsArray[loopCounter].deadline=programsArray[loopCounter].period;
						programsArray[loopCounter].state = STATE_READY;
						//pthread_mutex_unlock(&(programsArray[loopCounter].mutex));
					}

				}
			}

		}
	printf("If you reached here, the scheduler stopped running\n");

}

void * earliestDeadlineScheduler(void * arg){
	pthread_attr_t progThreadAttributes;
	struct sched_param progParameters;
	int policy;

	pthread_attr_init(&progThreadAttributes);
	pthread_getschedparam(pthread_self(),&policy, &progParameters);

	int shortest = 500;
	int nextShortest = 500;
	int found = 1;
	int threadNum=0;

	int loopCounter;
 	ProgramInfo buffer;
	int back;
	for(loopCounter = 1; loopCounter < numPrograms; loopCounter++){
		buffer= programsArray[loopCounter];
		back = loopCounter-1;
		while(back >= 0 && (programsArray[loopCounter].deadline < buffer.deadline)){
			programsArray[back+1]=programsArray[back];
			back--;
		}
		programsArray[back+1]=buffer;

	}

	for(loopCounter = 0; loopCounter < numPrograms; loopCounter++){
		progParameters.sched_priority--;

		pthread_attr_setschedparam(&progThreadAttributes, &progParameters);
		pthread_create(&(programsArray[loopCounter].threadID), &progThreadAttributes, &program, (void *)loopCounter);
	}

	for(;;){
		pid= MsgReceivePulse(chid, &pulse, sizeof(pulse),NULL);


			/*shortest
			 *next shortest
			 *first pass record shortest, give it highest priority
			 *next pass find next shortest, give it 1 lower priority, then make it shortest
			 *repeat until next shotest not found
			 *
			 */
			pthread_getschedparam(pthread_self(),&policy, &progParameters);

			for(loopCounter = 0; loopCounter < numPrograms; loopCounter++){
				if(programsArray[loopCounter].deadline  < shortest){
					shortest = programsArray[loopCounter].deadline;
					threadNum=loopCounter;

				}
			}
			pthread_setschedprio(programsArray[threadNum].threadID, progParameters.sched_priority--);
			loopCounter = 0;
			while(found){
				found = 0;
				for(loopCounter = 0;loopCounter < numPrograms;loopCounter++){
					if(programsArray[loopCounter].deadline < nextShortest && programsArray[loopCounter].deadline > shortest){
						nextShortest = programsArray[loopCounter].deadline;
						found = 1;
						threadNum = loopCounter;
					}
				}
				if(found){
					pthread_setschedprio(programsArray[threadNum].threadID, progParameters.sched_priority--);
				}
			}


	}

}

/*void * leastSlackTime(void * arg){


}*/

void ReadData(){
	char buffer[11];
	printf("Number of Programs?\n");
	fgets(buffer,sizeof(buffer),stdin);
	numPrograms = atoi(buffer);
	programsArray = (ProgramInfo *)malloc(numPrograms*sizeof(ProgramInfo));
	int i;
	for(i = 0; i < numPrograms; i++){
		printf("Runtime of Program %i?\n", i);
		fgets(buffer,sizeof(buffer),stdin);
		programsArray[i].runTime=atoi(buffer);

		printf("Period of Program %i?\n",i);
		fgets(buffer,sizeof(buffer),stdin);
		programsArray[i].period=atoi(buffer);

		printf("Deadline of Program %i?\n",i);
		fgets(buffer,sizeof(buffer),stdin);
		programsArray[i].deadline=atoi(buffer);

		if(programsArray[i].period != programsArray[i].deadline){
			perror("Deadline Not Equal to Period!\n");
			exit(EXIT_FAILURE);
		}

		programsArray[i].state=STATE_READY;
		pthread_mutex_init(&(programsArray[i].mutex),NULL);
		pthread_mutex_unlock(&(programsArray[i].mutex));
	}

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
	pthread_attr_setdetachstate(&threadAttributes, PTHREAD_CREATE_JOINABLE);
	pthread_attr_setschedparam(&threadAttributes, &parameters);

	pthread_mutex_init(&mutex,NULL);
	nanospin_calibrate(0);

	/*Real Time Clock Setup*/
	clkper.nsec = 1000000;
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
	timer.it_value.tv_nsec=1000000;
	timer.it_interval.tv_sec=0;
	timer.it_interval.tv_nsec=1000000;

	timer_settime( timer_id, 0, &timer, NULL );

	ReadData();

	pthread_cond_init(&control, NULL);



	pthread_create(&scheduleThreadID,&threadAttributes, &rateMonotonicScheduler, (void *)1);
	pthread_join(scheduleThreadID,NULL);
	while(1){

	}




	return EXIT_SUCCESS;

}


