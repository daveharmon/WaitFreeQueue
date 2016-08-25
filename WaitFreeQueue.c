/*
 *
 */

#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>

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

WFQueue_t* wait_free_queue_init(int num_threads)
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
	return q;
}

void wait_free_queue_destroy(WFQueue_t* q)
{
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
		printf("0\n");
		if (last == ((queue_node_t*)atomic_load(&q->tail))) {
			printf("1\n");
			if (NULL == next) {	// enqueue can be applied
				printf("2\n");
				if (is_still_pending(q, tid, phase)) {
					printf("3\n");
					if (atomic_compare_exchange_strong(&last->next, 
							(intptr_t*)&next, 
							(intptr_t)((queue_op_desc_t*)atomic_load((atomic_intptr_t*)q->state[tid]))->node)) {
						printf("4\n");
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

int help_deq(WFQueue_t* q, int tid, long phase)
{
	return 0;
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

void wf_enqueue(WFQueue_t* q, int tid, int value)
{
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

void help_deq()
{

}

void help_finish_deq()
{
	
}

int wf_dequeue(WFQueue_t* q, int tid)
{
	long phase = max_phase(q) + 1;
	q->state[tid] = queue_node_init(phase, true, false, null);
	help(q, phase);
	help_finish_deq();
	queue_node_t* node = q->state[tid]->node;
	if (NULL == node)
	{
		printf("EMPTY EXCEPTION\n");
		return;
	}
	return atomic_load(node->next)->value;
}


