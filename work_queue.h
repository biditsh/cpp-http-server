#ifndef __WQ_
#define __WQ_

#include <pthread.h>
#include <stdio.h>

/*pthread lock variables*/
pthread_mutex_t qMutex;
pthread_cond_t qEmpty;

typedef struct tdata {
  int clientd;
  char * prog;
  int port;
  int request_id;
  FILE * logFile;
  struct tdata * link;
} tdata;

struct work_queue{
	struct tdata *start;
	struct tdata ** end;
} work_queue;

void addq(struct tdata *tdatap);
struct tdata *removeq();



#endif