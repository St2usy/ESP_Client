#ifndef _QUEUE_THREAD_H_
#define _QUEUE_THREAD_H_

#include <stdio.h>
#include <stdlib.h>


/*************************************************************************************
	For Queue
 *************************************************************************************/
typedef struct _node {
	void* message;
	struct _node* next_node;
} NODE;

typedef NODE* NODE_PTR;

typedef struct _queue {
	int size;
	NODE_PTR first;
	NODE_PTR last;
} QUEUE;

QUEUE* create_queue(void);
void destroy_queue(QUEUE* q_ptr);

void add_queue(QUEUE* q_ptr, void* message);
void delete_queue(QUEUE* q_ptr);

#endif