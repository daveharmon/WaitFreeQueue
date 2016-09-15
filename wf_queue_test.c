/* 
 *
 */

#include <pthread.h>
#include <stdlib.h>

#include "WaitFreeQueue.c"

struct pthread_start 
{
	int tid;
};

struct pthread_ret
{
	int tid;
	char* status;
};

WFQueue* q;

void* wf_queue_test(void* args) 
{
	struct pthread_start* pthread_arg = (struct pthread_start*)args;
	int tid = pthread_arg->tid;
	printf("tid: %i\n", tid);
	queue_op_desc_t* op = (queue_op_desc_t*)atomic_load(
		(atomic_intptr_t*)q->state[tid]);
	printf("%i loaded: %lu with phase: %ld\n", tid, (long)op, op->phase);
	printf("is_still_pending: %i\n", is_still_pending_test(q, tid, -1));

	struct pthread_ret* ret = malloc(sizeof(struct pthread_ret));
	ret->tid = tid;
	ret->status = "SUCCESS!";
	return (void*)ret;

	wf_enqueue(q, tid, tid*5);
	op = (queue_op_desc_t*)atomic_load(
		(atomic_intptr_t*)q->state[tid]);
	printf("%i loaded: %lu with phase: %ld\n", tid, (long)op, op->phase);
	printf("is_still_pending: %i\n", is_still_pending_test(q, tid, 0));

	return NULL;
}


 int main()
{
	printf("testing queue_op_desc_init\n");
	queue_op_desc_t* p = queue_op_desc_init(-1, 0, 1, NULL);
	printf("phase = %ld\n", p->phase);
	printf("done testing queue_op_desc_init\n\n");

	printf("About to initialize q\n");
	q = wait_free_queue_init(3);

	pthread_t** threads = calloc(3, sizeof(pthread_t));
	for (int i = 0; i < 3; i++)
	{
		struct pthread_start* args = malloc(sizeof(struct pthread_start));
		args->tid = i;

		if (pthread_create(threads[i], NULL, &wf_queue_test, (void *)args))
		{
			printf("Created pthread\n");
		}

		free(args);
		printf("got here\n");
	}



	for (int i = 0; i < 3; i++)
	{
		struct pthread_ret* ret;
		pthread_join(*threads[i], (void*)&ret);
		printf("%i: %s\n", ret->tid, ret->status);
		free(ret);
	}

	printf("About to destroy q\n");
	wait_free_queue_destroy(q);
	free(threads);
	exit(0);
}

