#include "queue.h"

#include <stdio.h>
#include <stdlib.h>

// queue - max size
#define MAX_QUEUE_SIZE 100

QUEUE* create_queue()
{
	QUEUE* q = (QUEUE*) malloc (sizeof(QUEUE));

	if(q == NULL) { return NULL; }

	q->size = 0;
	q->first = NULL;
	q->last = NULL;

	return q;
}

void destroy_queue(QUEUE* q_ptr)
{
	if(q_ptr != NULL)
	{
		while(q_ptr->size > 0)
		{
			delete_queue(q_ptr);
		}
		free(q_ptr);
	}
}

void add_queue(QUEUE* q_ptr, void* message)
{
	if(q_ptr->size >= MAX_QUEUE_SIZE)
	{
		fprintf(stderr, "[QUEUE] Full. max queue size : (%d)\n", MAX_QUEUE_SIZE);
		return;
	}

	NODE_PTR new = (NODE_PTR) malloc (sizeof(NODE));
	new->message = message;
	new->next_node = NULL;

	if(q_ptr->size == 0) {
		q_ptr->first = new;
		q_ptr->last = new;
	} else {
		q_ptr->last->next_node = new;
		q_ptr->last = new;
	}

	(q_ptr->size)++;

	// printf("ADD >> Q size:%d\n", q_ptr->size);
}

void delete_queue(QUEUE* q_ptr)
{
	if(q_ptr->size == 0) {
		fprintf(stderr, "[WARN] Q is empty. no need to delete.\n");
	} else {
		NODE_PTR temp = q_ptr->first;
		q_ptr->first = temp->next_node;
		free(temp);
		temp = NULL;
		(q_ptr->size)--;
	}
	// printf("DEL >> Q size:%d\n", q_ptr->size);
}


