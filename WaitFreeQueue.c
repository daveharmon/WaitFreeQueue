/*
 *	C implementation of a wait free queue based on 
 *	http://www.cs.technion.ac.il/~erez/Papers/wfquque-ppopp.pdf
 *	
 */

#include "WaitFreeQueue.h"

#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>

/*** ------------ Private Structures ------------ ***/

typedef struct 
{
	int value;
	atomic_intptr_t next;
	int enqTid;
	atomic_int deqTid;
} queue_node_t;

typedef struct 
{
	long phase;
	int pending;
	int enqueue;
	queue_node_t* node;
} queue_op_desc_t;

typedef struct  
{
	queue_node_t* sentinel;
	atomic_intptr_t head;
	atomic_intptr_t tail;
	void** state;
	atomic_int length;
	int space;
} WFQueue_t;

/*** ------------ Private Functions ------------ ***/


queue_node_t* queue_node_init(int val, int etid)
{
	queue_node_t* node = malloc(sizeof(queue_node_t));
	node->value = val;
	atomic_init(&node->next, 0);
	node->enqTid = etid;
	atomic_init(&node->deqTid, -1);
	return node;
}

void queue_node_destroy(queue_node_t* node)
{
	free(node);
}

queue_op_desc_t* queue_op_desc_init(long ph, int pend, int enq, queue_node_t* n)
{
	queue_op_desc_t* op = malloc(sizeof(queue_op_desc_t));
	op->phase = ph;
	op->pending = pend;
	op->enqueue = enq;
	op->node = n;
	return op;
}

void queue_op_desc_destroy(queue_op_desc_t* op)
{
	free(op);
}

int is_still_pending(WFQueue_t* q, int tid, long ph) 
{
	queue_op_desc_t* p = (queue_op_desc_t*)atomic_load(
		(atomic_intptr_t*)q->state[tid]);
	return (p->pending && p->phase) <= ph;
}

void help_finish_enq(WFQueue_t* q)
{
	queue_node_t* last = (queue_node_t*)atomic_load(&q->tail);
	queue_node_t* next = (queue_node_t*)atomic_load(&last->next);
	if (NULL != next) {
		int tid = next->enqTid;
		queue_op_desc_t* curDesc = (queue_op_desc_t*)atomic_load((atomic_intptr_t*)&q->state[tid]);
		if ((last == (queue_node_t*)atomic_load(&q->tail)) &&
			 (next == ((queue_op_desc_t*)atomic_load((atomic_intptr_t*)q->state[tid]))->node)) {
			queue_op_desc_t* newDesc = queue_op_desc_init(
				((queue_op_desc_t*)atomic_load((atomic_intptr_t*)&q->state[tid]))->phase, 0, 1, next);
			atomic_compare_exchange_strong((atomic_intptr_t*)q->state[tid], (intptr_t*)&curDesc, (intptr_t)newDesc);
			atomic_compare_exchange_strong(&q->tail, (intptr_t*)&last, (intptr_t)next);
		}
	}
}

int help_enq(WFQueue_t* q, int tid, long phase)
{
	while (is_still_pending(q, tid, phase)) {
		queue_node_t* last = (queue_node_t*)atomic_load(&q->tail);
		queue_node_t* next = (queue_node_t*)atomic_load(&last->next);
		if (last == ((queue_node_t*)atomic_load(&q->tail))) {
			if (NULL == next) {	// enqueue can be applied
				if (is_still_pending(q, tid, phase)) {
					if (atomic_compare_exchange_strong(&last->next, 
							(intptr_t*)&next, 
							(intptr_t)((queue_op_desc_t*)atomic_load((atomic_intptr_t*)q->state[tid]))->node)) {
					help_finish_enq(q);
						return 0;
					}
				}
			} else {	// some enqueue op is in progress, and may need help
				help_finish_enq(q);
			}
		}
	}
	return 0;
}

void help_finish_deq(WFQueue_t* q)
{
	queue_node_t* first = (queue_node_t*)atomic_load(&q->head);
	queue_node_t* next = (queue_node_t*)atomic_load(&first->next);
	int tid = atomic_load(&first->deqTid);	// read deqTid of the first element
	if (-1 != tid)
	{
		queue_op_desc_t* curDesc = (queue_op_desc_t*)atomic_load((atomic_intptr_t*)q->state[tid]);
		if ((first == (queue_node_t*)atomic_load(&q->head)) && (NULL != next)) 
		{
			queue_op_desc_t* newDesc = queue_op_desc_init(((queue_op_desc_t*)atomic_load(
				(atomic_intptr_t*)q->state[tid]))->phase, 0, 0, 
			((queue_op_desc_t*)atomic_load((atomic_intptr_t*)q->state[tid]))->node);
			atomic_compare_exchange_strong((atomic_intptr_t*)q->state[tid], 
				(intptr_t *)&curDesc, (intptr_t)newDesc);
			atomic_compare_exchange_strong(&q->head, (intptr_t *)&first, (intptr_t)next);
		}
	}
}

void help_deq(WFQueue_t* q, int tid, long phase)
{
	while (is_still_pending(q, tid, phase))
	{
		queue_node_t* first = (queue_node_t*)atomic_load(&q->head);
		queue_node_t* last = (queue_node_t*)atomic_load(&q->tail);
		queue_node_t* next = (queue_node_t*)atomic_load(&first->next);
		if (first == (queue_node_t*)atomic_load(&q->head))
		{
			if (first == last) 	// queue might be empty
			{
				if (NULL == next)	// queue is empty
				{
					queue_op_desc_t* curDesc = (queue_op_desc_t*)atomic_load((atomic_intptr_t*)q->state[tid]);
					if ((last == (queue_node_t*)atomic_load(&q->tail)) && (is_still_pending(q, tid, phase)))
					{
						queue_op_desc_t* newDesc = queue_op_desc_init(
							((queue_op_desc_t*)atomic_load((atomic_intptr_t*)q->state[tid]))->phase, 0, 0, NULL);
						atomic_compare_exchange_strong((atomic_intptr_t*)q->state[tid], (intptr_t *)&curDesc, (intptr_t)newDesc);
					}
				}
				else	// some enqueue is in progress 
				{
					help_finish_enq(q);	// help it first, then retry
				}
			}	
			else	// queue is not empty
			{
				queue_op_desc_t* curDesc = (queue_op_desc_t*)atomic_load((atomic_intptr_t*)q->state[tid]);
				queue_node_t* node = curDesc->node;
				if (!is_still_pending(q, tid, phase)) break;
				if (((intptr_t)first == atomic_load(&q->head)) && node != first)
				{
					queue_op_desc_t* newDesc = queue_op_desc_init(
						((queue_op_desc_t*)atomic_load((atomic_intptr_t*)q->state[tid]))->phase, 1, 0, first);
					if (!atomic_compare_exchange_strong((atomic_intptr_t*)q->state[tid], (intptr_t *)&curDesc, (intptr_t)newDesc))
					{
						continue;
					}
				}
				int new_value = -1;
				atomic_compare_exchange_strong(&first->deqTid, &new_value, tid);
				help_finish_deq(q);
			}
		}
	}
}

void help(WFQueue_t* q, long phase)
{
	for (int i = 0; i < q->space; i++) {
		queue_op_desc_t* desc = (queue_op_desc_t*)atomic_load(
			(atomic_intptr_t*)q->state[i]);
		if ((desc->pending && desc->phase) <= phase) {
			if (desc->enqueue) {
				help_enq(q, i, phase);
			} else {
				help_deq(q, i, phase);
			}
		}
	}
}

long max_phase(WFQueue_t* q)
{
	long max_phase = -1;
	for (int i = 0; i < q->length; i++) {
		long phase = ((queue_op_desc_t*)atomic_load(
			(atomic_intptr_t*)q->state[i]))->phase;
		if (phase > max_phase) {
			max_phase = phase;
		}
	}
	return max_phase;
}

/*** ------------ Public Functions ------------ ***/

WFQueue* wait_free_queue_init(int num_threads)
{
	WFQueue_t* q = malloc(sizeof(WFQueue_t));
	q->sentinel = queue_node_init(-1, -1);
	atomic_init(&q->head, (long)q->sentinel);
	atomic_init(&q->tail, (long)q->sentinel);
	atomic_init(&q->length, 0); 
	q->space = num_threads;

	q->state = malloc(sizeof(void*) * num_threads);
	for (int i = 0; i < num_threads; i++)
	{
		atomic_intptr_t* ptr = malloc(sizeof(atomic_intptr_t));
		atomic_init(ptr, (intptr_t)queue_op_desc_init(-1, 0, 1, NULL));
		q->state[i] = ptr;
	}
	return (WFQueue*)q;
}

void wait_free_queue_destroy(WFQueue* wf_q)
{
	WFQueue_t* q = (WFQueue_t*)wf_q;
	for (int i = 0; i < q->space; i++) {
		if (((queue_op_desc_t*)atomic_load(
			(atomic_intptr_t*)q->state[i]))->node) {
			queue_node_destroy(((queue_op_desc_t*)atomic_load(
			(atomic_intptr_t*)q->state[i]))->node);
		}
		queue_op_desc_destroy((queue_op_desc_t*)atomic_load(
			(atomic_intptr_t*)q->state[i]));
	}
	free(q->state);
	free(q);
}

int is_still_pending_test(WFQueue* wf_q, int tid, long ph) 
{
	WFQueue_t* q = (WFQueue_t*)wf_q;
	queue_op_desc_t* p = (queue_op_desc_t*)atomic_load(
		(atomic_intptr_t*)q->state[tid]);
	return (p->pending && p->phase) <= ph;
}

void wf_enqueue(WFQueue* wf_q, int tid, int value)
{
	WFQueue_t* q = (WFQueue_t*)wf_q;
	long phase = max_phase(q) + 1;
	queue_op_desc_t* op = (queue_op_desc_t*)atomic_load(
		(atomic_intptr_t*)q->state[tid]);
	op->phase = phase;
	op->pending = 1;
	op->enqueue = 1;
	op->node = queue_node_init(value, tid);
	help(q, phase);
	help_finish_enq(q);
}

int wf_dequeue(WFQueue* wf_q, int tid)
{
	WFQueue_t* q = (WFQueue_t*)wf_q;
	long phase = max_phase(q) + 1;
	q->state[tid] = queue_op_desc_init(phase, 1, 0, NULL);
	help(q, phase);
	help_finish_deq(q);
	queue_node_t* node = ((queue_op_desc_t*)atomic_load(
		((atomic_intptr_t*)q->state[tid])))->node;
	if (NULL == node)
	{
		printf("EMPTY EXCEPTION\n");
		return -1;
	}
	return ((queue_node_t*)atomic_load(&node->next))->value;
}


