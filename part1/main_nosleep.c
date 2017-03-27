#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/ioctl.h>
#include <time.h>
#include <math.h>
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include "commonData.h"


int randomValueGen(int limit);
ht_object_t create_ht_Object(int k,int v);
void* threadFunction(void *p);
void dump_table();
void tab_printer(int t_no);
pthread_mutex_t i_mutex = PTHREAD_MUTEX_INITIALIZER;

ht_object_t create_ht_Object(int k,int v){
	ht_object_t object;
	object.key = k;
	object.data = v;
	return object;
}


int main(int argc, char **argv){

	pthread_t thread1, thread2, thread3, thread4;
	pthread_attr_t tattr1,tattr2,tattr3,tattr4;
	int prio_1 = 20;
	struct sched_param param1,param2,param3,param4;
	int seed1 = 0, seed2 = 100,seed3 = 200,seed4 = 300;
	int  iret1, iret2, iret3, iret4;
	time_t t;

	srand((unsigned) time(&t));

	pthread_attr_init (&tattr1);
	pthread_attr_init (&tattr2);
	pthread_attr_init (&tattr3);
	pthread_attr_init (&tattr4);

	pthread_attr_setschedpolicy(&tattr1, SCHED_FIFO);
	pthread_attr_setschedpolicy(&tattr2, SCHED_FIFO);
	pthread_attr_setschedpolicy(&tattr3, SCHED_FIFO);
	pthread_attr_setschedpolicy(&tattr4, SCHED_FIFO);

	pthread_attr_getschedparam (&tattr1, &param1);
	pthread_attr_getschedparam (&tattr2, &param2);
	pthread_attr_getschedparam (&tattr3, &param3);
	pthread_attr_getschedparam (&tattr4, &param4);

	param1.sched_priority = prio_1;
	param2.sched_priority = prio_1 + 20;
	param3.sched_priority = prio_1 + 60;
	param4.sched_priority = prio_1 + 70;

	pthread_attr_setschedparam (&tattr1, &param1);
	pthread_attr_setschedparam (&tattr2, &param2);
	pthread_attr_setschedparam (&tattr3, &param3);
	pthread_attr_setschedparam (&tattr4, &param4);

	iret1 = pthread_attr_setinheritsched(&tattr1,PTHREAD_EXPLICIT_SCHED);
	if(iret1){
		printf("Error - set inheritence() return code: %d\n",iret1);
		perror("Error: ");
	}
	iret2 = pthread_attr_setinheritsched(&tattr2,PTHREAD_EXPLICIT_SCHED);
	iret3 = pthread_attr_setinheritsched(&tattr3,PTHREAD_EXPLICIT_SCHED);
	iret4 = pthread_attr_setinheritsched(&tattr4,PTHREAD_EXPLICIT_SCHED);


 	iret1 = pthread_create( &thread1, &tattr1, threadFunction, (void*) &seed1);
	if(iret1){
		printf("Error - pthread_create() return code: %d\n",iret1);
		perror("Error: ");
	}
	iret2 = pthread_create( &thread2, &tattr2, threadFunction, (void*) &seed2);
	if(iret2){
		printf("Error - pthread_create() return code: %d\n",iret2);
		perror("Error: ");
	}
	iret3 = pthread_create( &thread3, &tattr3, threadFunction, (void*) &seed3);
	if(iret3){
		printf("Error - pthread_create() return code: %d\n",iret3);
		perror("Error: ");
	}
	iret4 = pthread_create( &thread4, &tattr4, threadFunction, (void*) &seed4);
	if(iret4){
		printf("Error - pthread_create() return code: %d\n",iret4);
		perror("Error: ");
	}


	pthread_join(thread1, NULL);
	pthread_join(thread2, NULL);
	pthread_join(thread3, NULL);
	pthread_join(thread4, NULL);

	dump_table();

	return 0;
}

void* threadFunction(void *p){
	int seed = *((int *)p); 
	ht_object_t tempw_object,tempr_object;
	int n,res,fd;
	int thread_no = (seed/100) + 1;
	
	struct dump_arg arg;

	printf("Thread %d starting\n",thread_no);
	//file open
	fd = open("/dev/ht530", O_RDWR);
	if (fd < 0 ){
		printf("Can not open device file.\n");		
		return 0;
	}

	//50 writes	
	for(n=1;n<=50;n++){
		pthread_mutex_lock(&i_mutex);

		tempw_object = create_ht_Object(n,(n*2)+seed);
		res = write(fd,&tempw_object,sizeof(ht_object_t));

		pthread_mutex_unlock(&i_mutex);
		//usleep(randomValueGen(100)*1000);
	}

	pthread_mutex_lock(&i_mutex);
	tab_printer(thread_no);
	printf("WRITE OVER, THREAD %d\n",thread_no);
	pthread_mutex_unlock(&i_mutex);

	//50 reads
	for(n=1;n<=50;n++){
		pthread_mutex_lock(&i_mutex);
		tempr_object = create_ht_Object(n,0);
		res = read(fd,&tempr_object,sizeof(ht_object_t));
		tab_printer(thread_no);
		if(res == -1){
			printf("key %d does not exist, thread %d\n", n, thread_no);
		}else{
			printf("key =%d value=%d thread=%d \n",tempr_object.key,tempr_object.data,thread_no);
		}
		pthread_mutex_unlock(&i_mutex);
		//usleep(randomValueGen(100)*1000);
	}

	//delete from key 50
	pthread_mutex_lock(&i_mutex);
	tempw_object.key = 50;
	tempw_object.data = 0;
	res = write(fd,&tempw_object,sizeof(ht_object_t));
	pthread_mutex_unlock(&i_mutex);

	//read from key 50
	pthread_mutex_lock(&i_mutex);
	tempr_object.key = 50;
	tempr_object.data = 0;
	res = read(fd,&tempr_object,sizeof(ht_object_t));
	tab_printer(thread_no);
	if(res == -1){
		printf("reading from key 5 does not exist, thread %d\n",thread_no);
	}else{
		printf("key =%d value=%d thread=%d \n",tempr_object.key,tempr_object.data,thread_no);
	}
	pthread_mutex_unlock(&i_mutex);

	pthread_mutex_lock(&i_mutex);
	tab_printer(thread_no);
	printf("READ OVER, THREAD %d\n",thread_no);
	pthread_mutex_unlock(&i_mutex);

	//close file
	close(fd);

}

int randomValueGen(int limit){
	int value;
	value = rand()%limit;
	return value;
	
}

void dump_table(){
	int fd;
	int bkt,res,n;
	struct dump_arg arg;

	printf("\n......Table Dump.......\n");
	fd = open("/dev/ht530", O_RDWR);
	if (fd < 0 ){
		printf("Can not open device file.\n");		
		return;
	}

	for(bkt =0;bkt<128; bkt++){
		arg.n = bkt;
		res = ioctl(fd,dump,&arg);
		if(res > 0){
			printf("\nbucket %d contents:",arg.n);
			for(n=0;n<(res);n++){
				printf("%d ",arg.object_array[n].data);
			}
			printf("\n");
		}
	}


}

void tab_printer(int t_no){
	switch(t_no){
		case 1: break;
		case 2: printf("\t\t\t\t");
				break;
		case 3: printf("\t\t\t\t\t\t\t\t");
				break;
		case 4: printf("\t\t\t\t\t\t\t\t\t\t\t\t");
				break;
	}
}