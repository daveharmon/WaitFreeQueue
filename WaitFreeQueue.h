/*
 *	C implementation of a wait free queue based on 
 *	http://www.cs.technion.ac.il/~erez/Papers/wfquque-ppopp.pdf
 *	
 *	A wait free queue is a concurrent data FIFO data structure such that
 *	a process or thread completes its operations in a bounded number of steps
 * 	regardless of what other processes or threads are doing.  
 *	
 *	Such a structure is useful in many contexts where strict deadlines for 
 *	operational completion exist, such as in real-time applications or when
 * 	operating under a strict SLA, or in heterogenous execution environments
 *	where some threads perform much faster than others.
 *
 *	Wait freedom is distinct from "Lock freedom".  Lock freedom is a weaker 
 *	property that ensures that among all processes accessing a queue, at least
 *	one will succeed to finish its operation.  This guarentees global progress,
 *	but allows for scenarios in which all but one thread starve while trying
 *	to execute an operation on the queue.  Thus losing much of the advantags
 *	of a concurrent data structure.
 *
 */

typedef struct WFQueue WFQueue;

/*
 *
 */
WFQueue* wait_free_queue_init(int num_threads);

/*
 *
 */
void wait_free_queue_destroy(WFQueue* q);

/*
 *
 */
void wf_enqueue(WFQueue* q, int tid, int value);

/*
 *
 */
int wf_dequeue(WFQueue* q, int tid);
