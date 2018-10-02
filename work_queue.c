#include "work_queue.h"



/* addq
     1 argument:  address of a tdata item
     State change:  The tdata item arg1 is appended to the end of 
	work_queue, adjusting work_queue's members to reflect the new item
     Return: none */
void addq(struct tdata *tdatap) {
  pthread_mutex_lock(&qMutex);
  if (work_queue.end == &work_queue.start)
    pthread_cond_broadcast(&qEmpty);
  *work_queue.end = tdatap;
  tdatap->link = 0;
  work_queue.end = &tdatap->link;
  pthread_mutex_unlock(&qMutex);
  return;
}

/* removeq
     No arguments
     State change:  If work_queue contains at least one item,
        remove the first item from work_queue, adjusting  work_queue 
        to reflect that removal. 
     Return: The address of the removed struct tdata item, or 0 if 
        work_queue was empty. */
struct tdata *removeq() {
  pthread_mutex_lock(&qMutex);
  while (work_queue.start == 0)
    /* assert: there are no elements in work_queue */
    pthread_cond_wait(&qEmpty, &qMutex);  
  /* assert:  there is at least one element in work_queue */
  struct tdata *tmp = work_queue.start;
  work_queue.start = tmp->link;
  if (work_queue.start == 0)
    /* assert:  no remaining elements in work_queue */
    work_queue.end = &work_queue.start;
  pthread_mutex_unlock(&qMutex);
  tmp->link = 0;
  return tmp;
}

