
// Compile Using:
// sudo gcc -o program RMSLAB.c -lpthread
// Run the compiled program using:
// sudo ./program

#define _GNU_SOURCE // Must be placed first in the file

#include <stdio.h>
#include <math.h>
#include <time.h>

#include "tick.h"
#include "BrickPi.h"
#include <pthread.h>
#include <linux/i2c-dev.h>  
#include <fcntl.h>
#include <unistd.h>

int turnSignal = 0;
int result;

#undef DEBUG

void *wheelMotor();
void *rgbSensor();
void *pushSensorTurnLeft();
void *pushSensorTurnRight();
void *distanceSensor();
enum commandenum{STOP,FORWARD,BACK,LEFT,RIGHT };
void timespec_add_us(struct timespec *t, long us);
int timespec_cmp(struct timespec *a, struct timespec *b);

void order_update(int u, int d, enum commandenum c, int s);

int clock_getres(clockid_t clock_id, struct timespec *res);
int clock_gettime(clockid_t clock_id, struct timespec *tp);
int clock_settime(clockid_t clock_id, const struct timespec *tp);
int pthread_getcpuclockid(pthread_t thread_id, clockid_t *clock_id);

unsigned sleep(unsigned seconds);
int nanosleep(const struct timespec *rqtp, struct timespec *rmtp);

int clock_nanosleep(clockid_t clock_id, int flags,
const struct timespec *rqtp, struct timespec *rmtp);

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;


struct order
{
int urgent_level;
int duration;
enum commandenum command;
int speed;
}order_status;



struct periodic_data {
	int index;
	long period_us;
	int wcet_sim;
};

int main() {
  ClearTick();

  pthread_t motorThread, rgbThread, distanceThread, pushThreadTurnLeft, pushThreadTurnRight;
  pthread_attr_t my_attr;
  
  struct sched_param param;
  
  result = BrickPiSetup();
  printf("BrickPiSetup: %d\n", result);
  if(result) 
    return 0;

  BrickPi.Address[0] = 1;
  BrickPi.Address[1] = 2;

  BrickPi.MotorEnable[PORT_A] = 1;
  BrickPi.MotorEnable[PORT_B] = 1;

  BrickPi.SensorType[PORT_1] = TYPE_SENSOR_TOUCH;
  BrickPi.SensorType[PORT_2] = TYPE_SENSOR_TOUCH;
  BrickPi.SensorType[PORT_3] = TYPE_SENSOR_ULTRASONIC_CONT;
  BrickPi.SensorType[PORT_4] = TYPE_SENSOR_COLOR_FULL;

  pthread_attr_init(&my_attr);
  pthread_attr_setschedpolicy(&my_attr, SCHED_FIFO);
  
  param.sched_priority = 1;
  pthread_attr_setschedparam(&my_attr, &param);
  pthread_create(&motorThread, &my_attr, wheelMotor,NULL);
  
  param.sched_priority = 2;
  pthread_attr_setschedparam(&my_attr, &param);
  pthread_create(&rgbThread, &my_attr, rgbSensor,NULL);
  
  param.sched_priority = 2;
  pthread_attr_setschedparam(&my_attr, &param);
  pthread_create(&distanceThread, &my_attr, distanceSensor,NULL);
  
  param.sched_priority = 3;
  pthread_attr_setschedparam(&my_attr, &param);
  pthread_create(&pushThreadTurnLeft, &my_attr, pushSensorTurnLeft,NULL);
  
  param.sched_priority = 3;
  pthread_attr_setschedparam(&my_attr, &param);
  pthread_create(&pushThreadTurnRight, &my_attr, pushSensorTurnRight,NULL);
		
  pthread_attr_destroy(&my_attr);
  result = BrickPiSetupSensors();
  printf("BrickPiSetupSensors: %d\n", result); 

  if(!result){
    result = BrickPiUpdateValues();

    
    while(1){
      result = BrickPiUpdateValues();
    }
    
    
    //pthread_join(motorThread, NULL);
    //pthread_join(rgbThread, NULL);
    //pthread_join(distanceThread, NULL);
    //pthread_join(pushThreadTurnLeft, NULL);
    //pthread_join(pushThreadTurnRight, NULL);
  }
  return 0;
}

void *wheelMotor(){
	cpu_set_t cpuset;// Must be placed first in each thread function except main
	CPU_ZERO(&cpuset);
	CPU_SET(0,&cpuset);
	pthread_setaffinity_np(pthread_self(),sizeof(cpu_set_t),&cpuset); 
	
	struct periodic_data period;
	period.index = 0;
	period.period_us = 100000;
	period.wcet_sim	= 0;
	
	struct timespec next;
	struct timespec now;
	
	time_t t;
	srand((unsigned) time(&t));

	
	while(1){
		
		clock_gettime(CLOCK_REALTIME, &next);
		
		timespec_add_us(&next, period.period_us);
		
		switch (order_status.command)
		{
			case STOP:
				BrickPi.MotorSpeed[PORT_A]=order_status.speed;
				BrickPi.MotorSpeed[PORT_B]=order_status.speed;
			break;
			
			case FORWARD:
				BrickPi.MotorSpeed[PORT_A]=order_status.speed;
				BrickPi.MotorSpeed[PORT_B]=order_status.speed;
			break;
			
			case BACK:
				BrickPi.MotorSpeed[PORT_A]=-order_status.speed;
				BrickPi.MotorSpeed[PORT_B]=-order_status.speed;
			break;
			
			case LEFT:
				BrickPi.MotorSpeed[PORT_A]=-order_status.speed;
				BrickPi.MotorSpeed[PORT_B]=order_status.speed;
			break;
			
			case RIGHT:
				BrickPi.MotorSpeed[PORT_A]=order_status.speed;
				BrickPi.MotorSpeed[PORT_B]=-order_status.speed;
			break;
			
			default:
			break;
				
		}
		
		if(order_status.duration>0)
		{
			order_status.duration--;
		}
		else
		{
	
			int randNr = rand() % 15;
			printf("%d", randNr);
			
			switch (randNr)
			{
				case 1 :
					order_update(1,1,FORWARD, 200);
					break;
				case 2 :
					order_update(1,1,BACK, 200);
					break;
				case 3 :
					order_update(1,1,STOP, 0);
					break;
				case 4 :
					order_update(1,1,LEFT, 200);
					break;
				case 5 :
					order_update(1,1,RIGHT, 200);
					break;
				default:
					order_update(1,1,FORWARD, 200);
					break;
			}
			
			
		}
		 

		
		
		clock_gettime(CLOCK_REALTIME, &now);
		
		if (timespec_cmp(&now, &next) > 0) {
			fprintf(stderr, "Deadline miss for thread %d\n", period.index);
			fprintf(stderr, "now: %d sec %ld nsec next: %d sec %ldnsec \n",now.tv_sec, now.tv_nsec, next.tv_sec, next.tv_nsec);
			//exit(-1);
		}
		
		clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &next, NULL);

	}	
}

void *rgbSensor(){
	cpu_set_t cpuset;// Must be placed first in each thread function except main 
	CPU_ZERO(&cpuset);
	CPU_SET(0,&cpuset);
	pthread_setaffinity_np(pthread_self(),sizeof(cpu_set_t),&cpuset);

	struct periodic_data period;
	period.index = 0;
	period.period_us = 50000;
	period.wcet_sim	= 0;
	
	struct timespec next;
	struct timespec now;
	
	while(1){
		
		clock_gettime(CLOCK_REALTIME, &next);
		
		timespec_add_us(&next, period.period_us);
		
		char* col[] = {"t", "Black","Blue","Green","Yellow","Red","White"};
		//printf("RGB: %d %s\n", BrickPi.Sensor[PORT_4], col[BrickPi.Sensor[PORT_4]]);  //BrickPi.Sensor returns a number from 1 to 6. The col array contains the corresponding color names
		
		clock_gettime(CLOCK_REALTIME, &now);
		
		if (timespec_cmp(&now, &next) > 0) {
			fprintf(stderr, "Deadline miss for thread %d\n", period.index);
			fprintf(stderr, "now: %d sec %ld nsec next: %d sec %ldnsec \n",now.tv_sec, now.tv_nsec, next.tv_sec, next.tv_nsec);
			//exit(-1);
		}
		
		clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &next, NULL);

	}
		
}


void *pushSensorTurnLeft(){
	cpu_set_t cpuset;// Must be placed first in each thread function except main
	CPU_ZERO(&cpuset);
	CPU_SET(0,&cpuset);
	pthread_setaffinity_np(pthread_self(),sizeof(cpu_set_t),&cpuset); 
	struct periodic_data period;
	period.index = 0;
	period.period_us = 25000;
	period.wcet_sim	= 0;
	
	struct timespec next;
	struct timespec now;
	
	int leftCounter = 0;
	while(1){
		
		clock_gettime(CLOCK_REALTIME, &next);
		
		timespec_add_us(&next, period.period_us);
		
		if(BrickPi.Sensor[PORT_1] == 1){
			printf("pushSenorTurnLeft: %3.1d \n", BrickPi.Sensor[PORT_1] );
			leftCounter++;
			if (leftCounter > 5)
			{
				if(BrickPi.Sensor[PORT_2] == 1){
					order_update(3,1,BACK, 200);
				}else {
					order_update(3,1, LEFT,200);
				}
			}
		}else {
			leftCounter = 0;
		}
		
		clock_gettime(CLOCK_REALTIME, &now);
		
		if (timespec_cmp(&now, &next) > 0) {
			fprintf(stderr, "Deadline miss for thread %d\n", period.index);
			fprintf(stderr, "now: %d sec %ld nsec next: %d sec %ldnsec \n",now.tv_sec, now.tv_nsec, next.tv_sec, next.tv_nsec);
			//exit(-1);
		}
		
		clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &next, NULL);

	}
}

void *pushSensorTurnRight(){
	cpu_set_t cpuset;// Must be placed first in each thread function except main
	CPU_ZERO(&cpuset);
	CPU_SET(0,&cpuset);
	pthread_setaffinity_np(pthread_self(),sizeof(cpu_set_t),&cpuset); 
	
	struct periodic_data period;
	period.index = 0;
	period.period_us = 25000;
	period.wcet_sim	= 0;
	
	struct timespec next;
	struct timespec now;
	
	int rightCounter = 0;
	while(1){
		
		clock_gettime(CLOCK_REALTIME, &next);
		
		timespec_add_us(&next, period.period_us);
		
		if(BrickPi.Sensor[PORT_2] == 1){
			printf("pushSenorTurnRight: %3.1d \n", BrickPi.Sensor[PORT_2] );

			rightCounter++;
			if (rightCounter > 5)
			{
				if(BrickPi.Sensor[PORT_1] == 1){
					order_update(3,1,BACK, 200);
				}else {
					order_update(3,1, RIGHT,200);
				}
			}
		}else {
			rightCounter = 0;
		}

		clock_gettime(CLOCK_REALTIME, &now);
		
		if (timespec_cmp(&now, &next) > 0) {
			fprintf(stderr, "Deadline miss for thread %d\n", period.index);
			fprintf(stderr, "now: %d sec %ld nsec next: %d sec %ldnsec \n",now.tv_sec, now.tv_nsec, next.tv_sec, next.tv_nsec);
			//exit(-1);
		}
		
		clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &next, NULL);

	}
			
}

void *distanceSensor(){
	cpu_set_t cpuset;// Must be placed first in each thread function except main 
	CPU_ZERO(&cpuset);
	CPU_SET(0,&cpuset);
	pthread_setaffinity_np(pthread_self(),sizeof(cpu_set_t),&cpuset);


	struct periodic_data period;
	period.index = 0;
	period.period_us = 50000;
	period.wcet_sim	= 0;
	
	struct timespec next;
	struct timespec now;
	
	int distanceCounter = 0;
	while(1){
		
		clock_gettime(CLOCK_REALTIME, &next);
		
		timespec_add_us(&next, period.period_us);
		
		int val = BrickPi.Sensor[PORT_3];
		
		if(val!=255 && val!=127 && val != -1) {
			printf("%d\n", val);
			if(val <= 13){
				order_update(2,1,BACK,200);
				distanceCounter = 20;
			}
			else if (distanceCounter > 0)
			{
				order_update(2,1,BACK,200);
				distanceCounter--;
			}
			
		}else if(distanceCounter > 0) { 
			order_update(2,1,BACK,200);
			distanceCounter--;
		}
				
		clock_gettime(CLOCK_REALTIME, &now);
		
		if (timespec_cmp(&now, &next) > 0) {
			fprintf(stderr, "Deadline miss for thread %d\n", period.index);
			fprintf(stderr, "now: %d sec %ld nsec next: %d sec %ldnsec \n",now.tv_sec, now.tv_nsec, next.tv_sec, next.tv_nsec);
			//exit(-1);
		}
		
		clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &next, NULL);

	}
		
}

void order_update(int u, int d, enum commandenum c, int s)
{
	pthread_mutex_lock(&mutex);
	if(u>=order_status.urgent_level)
	{ 
		order_status.urgent_level=u;
		order_status.duration=d;
		order_status.command=c;
		order_status.speed=s;
	}
	pthread_mutex_unlock(&mutex);
}

void timespec_add_us(struct timespec *t, long us)
{
	t->tv_nsec += us*1000;
	if (t->tv_nsec > 1000000000) {
		t->tv_nsec = t->tv_nsec - 1000000000;// + ms*1000000;
		t->tv_sec += 1;
	}
}

int timespec_cmp(struct timespec *a, struct timespec *b)
{
	if (a->tv_sec > b->tv_sec) return 1;
	else if (a->tv_sec < b->tv_sec) return -1;
	else if (a->tv_sec == b->tv_sec) {
		if (a->tv_nsec > b->tv_nsec) return 1;
		else if (a->tv_nsec == b->tv_nsec) return 0;
		else return -1;
	}
}
