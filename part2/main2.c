#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/ioctl.h>
#include <time.h>
#include <math.h>
#include <stdint.h>
#include <errno.h>

#include "commonData.h"
#include "bufferStruct.h"

ht_object_t create_ht_Object(int k,int v){
	ht_object_t object;
	object.key = k;
	object.data = v;
	return object;
}

void printbuff_obj(buff_obj_t* buffer){
	printf("Trace Contents: \n");
	printf("PID: %d\n",buffer->tid);
	printf("local value: %d\t",buffer->l_value);
	printf("global var value: %d\n",buffer->g_value);
	printf("probe address: 0x%lx\n",buffer->kp_addr);
	printf("tsc: %lu\n\n", (long unsigned int) buffer->tsc);
}

void* probeThreadFunc(void* arg){
	int fd;
	int offset;
	int ret;
	size_t len;
	int seed;
	pthread_t probe_ht530;
	int  iret1;
	char ch;

	probe_arg_t kProbe_arg;

	printf("\n*****************\n");
	printf("Please Set Kprobe\n");

	fd = open("/dev/Mprobe", O_RDWR);
	if(fd < 0){
		printf("Can't open Mprobe\n");
		return 0;
	}

	/*
	*GATHERING KPROBE DATA
	*/
	memset(kProbe_arg.funcName, 0, NAME_LEN * sizeof(char));
	printf("Enter Function Name\n");
	fgets(kProbe_arg.funcName, NAME_LEN-1, stdin);
	printf("Enter Function offset in hex\n0x");
	fscanf(stdin, "%lx", &(kProbe_arg.funcOffset));
	kProbe_arg.funcName[strlen(kProbe_arg.funcName)-1] = '\0';

	printf("Enter function %s's local variable offset in hex\n0x", kProbe_arg.funcName);
	fscanf(stdin, "%lx", &(kProbe_arg.localOffset));

	printf("Enter global variable addressin hex\n0x");
	fscanf(stdin, "%lx", &(kProbe_arg.globalAddress));

	//flushing stdin
	while ((ch = getchar()) != '\n' && ch != EOF);
	/* 
	*KPROBE DATA GATHERED
	*/

	printf("\nkProbe args recd:\t funcName: %s \t function offset: 0x%lx\n", kProbe_arg.funcName, kProbe_arg.funcOffset);
	printf("\t\t\t local variable offset: 0x%lx \t global variable offset 0x%lx\n", kProbe_arg.localOffset, kProbe_arg.globalAddress);
	
	ret = write(fd, &kProbe_arg, sizeof(probe_arg_t));
	if(ret < 0){
		printf("Kprobe Registeration Failed\n");
		perror("Error: ");
		return 0;
	}
	printf("kProbe registered\n");
	close(fd);
}

void* threadFunction(void *arg){
	ht_object_t tempw_object,tempr_object;
	buff_obj_t buffer;
	int n,res,fd_ht,fd_mprobe;

	printf("\n*****************\n");
	printf("Testing kprobe\n");

	fd_ht = open("/dev/ht530", O_RDWR);
	if (fd_ht < 0 ){
		printf("Can not open ht530 device file.\n");		
		return 0;
	}
	fd_mprobe = open("/dev/Mprobe", O_RDWR);
	if (fd_mprobe < 0 ){
		printf("Can not open Mprobe device file.\n");		
		return 0;
	}

	printf("\n*****************\n");
	printf("Writing 25 objects to ht530 with key: 1 ... 25 and data: 111\n");
	for(n=1;n<=25;n++){
		tempw_object = create_ht_Object(n,111);
		res = write(fd_ht,&tempw_object,sizeof(ht_object_t));
		if(res < 0)
			perror("Error :");
	}

	printf("\nReading 10 objects from Mprobe. IF KPROBE IS IN HT_DRIVER_WRITE, TRACE DATA IS PRINTED\n");
	for(n=0;n<10;n++){
		res = read(fd_mprobe,&buffer,sizeof(buff_obj_t));
		if(res < 0){
			if(errno == EINVAL)
				printf("Trace Buffer Empty\n");
			else
				perror("Error :");
		}
		else
			printbuff_obj(&buffer);
	}

	printf("\n*****************\n");
	printf("Reading 25 objects from ht530 with key: 1 ... 25\n");
	for(n=1;n<=25;n++){
		tempr_object = create_ht_Object(n,0);
		res = read(fd_ht, &tempr_object, sizeof(ht_object_t));
		if(res == -1)
			printf("key %d does not exist\n", n);
		else
			printf("key =%d value=%d\n",tempr_object.key,tempr_object.data);
	}

	printf("\nReading 10 objects from Mprobe. IF KPROBE IS IN HT_DRIVER_READ, TRACE DATA IS PRINTED\n");
	for(n=0;n<10;n++){
		res = read(fd_mprobe,&buffer,sizeof(buff_obj_t));
		if(res < 0){
			if(errno == EINVAL)
				printf("Buffer Empty\n");
			else
				perror("Error :");
		}
		else
			printbuff_obj(&buffer);
	}

	close(fd_ht);
	close(fd_mprobe);
}

int main(int argc, char **argv){

	pthread_t thread1,thread2;
	int seed1 = 0, seed2 = 100;
	int  iret;

	iret = pthread_create( &thread1, NULL, probeThreadFunc, NULL);
	if(iret)
		printf("Error - pthread_create() return code: %d\n",iret);
	pthread_join(thread1, NULL);


	iret = pthread_create( &thread2, NULL, threadFunction, NULL);
	if(iret)
		printf("Error - pthread_create() return code: %d\n",iret);
	pthread_join(thread2, NULL);


	iret = pthread_create( &thread1, NULL, probeThreadFunc, NULL);
	if(iret)
		printf("Error - pthread_create() return code: %d\n",iret);
	pthread_join(thread1, NULL);


	iret = pthread_create( &thread2, NULL, threadFunction, NULL);
	if(iret)
		printf("Error - pthread_create() return code: %d\n",iret);
	pthread_join(thread2, NULL);

	return 0;
}
