#include <assert.h>
#include <stdlib.h>
#include <ucontext.h>
#include <stdbool.h>
#include "thread.h"
#include "interrupt.h"

/* This is the wait queue structure */
struct wait_queue
{
	/* ... Fill this in Lab 3 ... */
	//struct thread *head;
	// int threadID;
	// ucontext_t mycontext;
	// void *stack_ptr;
	// struct wait_queue *next; 
	struct thread *head;
};

struct thread *thread_queue_head;
struct exit_queue *exit_queue_head;
struct wait_queue *wait_queue_head;
int threads_count = 0;
bool existing_thread[THREAD_MAX_THREADS] = {false};

/*struct thread_queue {
	struct thread *TCB;
	struct thread_queue next;
};*/

/* This is the thread control block */
struct thread
{
	/* ... Fill this in ... */
	int threadID;
	ucontext_t mycontext;
	void *stack_ptr;
	struct thread *next;
};

struct exit_queue
{
	/* ... Fill this in ... */
	int threadID;
	ucontext_t mycontext;
	void *stack_ptr;
	struct exit_queue *next;
};

/*void print_queue(struct wait_queue *queue)
{
	printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~Starting printing queue~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
	struct thread *tempPTR = thread_queue_head;
	int i = 0;
	while (tempPTR != NULL)
	{
		printf("Thread number %d threadID %d\n", i, tempPTR->threadID);
		tempPTR = tempPTR->next;
		i++;
	}
	printf("!!!!!!!Starting printing KILL queue!!!!!!!!!!!\n");
	struct exit_queue *eq = exit_queue_head;
	while (eq != NULL)
	{
		printf("Kill Number: %d\n", eq->threadID);
		eq = eq->next;
	}
	printf("//////////////////Starting printing WAIT queue");
	struct wait_queue *wait = queue;
	while(wait != NULL) 
	{
		printf("Waiting threadID %d\n", wait->head->threadID);
		wait = wait->head->next;
	}
}
*/
/* thread starts by calling thread_stub. The arguments to thread_stub are the
 * thread_main() function, and one argument to the thread_main() function. */
void thread_stub(void (*thread_main)(void *), void *arg)
{
	//Tid ret;
	interrupts_on();
	thread_main(arg); // call thread_main() function with arg
	thread_exit();
}

/* Returns the head of the new list with first item moved to the back */
void push_to_back()
{
	struct thread *temp_ptr;
	temp_ptr = thread_queue_head;

	while (temp_ptr->next != NULL)
	{
		temp_ptr = temp_ptr->next;
	}

	temp_ptr->next = thread_queue_head;
	thread_queue_head = thread_queue_head->next;
	temp_ptr->next->next = NULL;
}

void push_to_desired(Tid want_tid)
{

	if (want_tid == thread_queue_head->threadID)
		return;

	push_to_back();

	//if(threads_count == 2) return;

	if (want_tid == thread_queue_head->threadID)
		return;

	struct thread *desired_prev = thread_queue_head;
	struct thread *desired = thread_queue_head;
	while (desired->threadID != want_tid)
	{
		desired_prev = desired;
		desired = desired->next;
	}
	desired_prev->next = desired_prev->next->next;
	desired->next = thread_queue_head;
	thread_queue_head = desired;
}

struct exit_queue *insert_to_exit_queue(Tid tid)
{
	struct thread *kill = thread_queue_head;
	struct thread *kill_prev = NULL;
	while (kill != NULL)
	{
		if (kill->threadID == tid)
		{
			if (kill_prev != NULL)
			{
				kill_prev->next = kill->next; // body
			}
			else
			{
				thread_queue_head = kill->next; // rem head
			}
			break;
		}
		kill_prev = kill;
		kill = kill->next;
	}
	kill->next = NULL;


	if (exit_queue_head == NULL)
	{
		exit_queue_head = malloc(sizeof(struct exit_queue));
		exit_queue_head->threadID = kill->threadID;
		exit_queue_head->stack_ptr = kill->stack_ptr;
		exit_queue_head->mycontext = kill->mycontext;
		kill->stack_ptr = NULL;
		existing_thread[exit_queue_head->threadID] = false;
		free(kill);
		threads_count--;
		return exit_queue_head;
	}

	struct exit_queue *insert_exit_queue = exit_queue_head;
	while (insert_exit_queue->next != NULL)
	{
		insert_exit_queue = insert_exit_queue->next;
	}
	insert_exit_queue->next = malloc(sizeof(struct exit_queue));
	insert_exit_queue->next->mycontext = kill->mycontext;
	insert_exit_queue->next->stack_ptr = kill->stack_ptr;
	insert_exit_queue->next->threadID = kill->threadID;
	insert_exit_queue->next->next = NULL;
	free(kill);

	existing_thread[insert_exit_queue->next->threadID] = false;
	threads_count--;

	return insert_exit_queue->next;
}

void thread_init(void)
{
	/* your optional code here */
	thread_queue_head = malloc(sizeof(struct thread));
	exit_queue_head = NULL;
	//current_thread = thread_queue_head;
	thread_queue_head->threadID = 0;

	thread_queue_head->next = NULL;
	thread_queue_head->stack_ptr = NULL;

	existing_thread[thread_queue_head->threadID] = true;
	threads_count++;

	int err;
	err = getcontext(&thread_queue_head->mycontext);
	assert(!err);
}

Tid thread_id()
{

	//TBD();
	if (thread_queue_head->threadID < 0 || thread_queue_head->threadID >= THREAD_MAX_THREADS)
		return THREAD_INVALID;
	else
		return thread_queue_head->threadID;
}

Tid thread_create(void (*fn)(void *), void *parg)
{
	int last_state = interrupts_off();

	if (threads_count == THREAD_MAX_THREADS)
	{
		interrupts_set(last_state);
		return THREAD_NOMORE;
	}

	threads_count++;

	struct thread *last = thread_queue_head;
	while (last->next != NULL)
	{
		last = last->next;
	}

	last->next = malloc(sizeof(struct thread));
	last = last->next;
	// last->mycontext = (ucontext_t*) malloc(sizeof(ucontext_t));
	last->next = NULL;

	for (int i = 0; i < THREAD_MAX_THREADS; i++)
	{
		if (existing_thread[i] == false)
		{
			last->threadID = i;
			break;
		}
	}
	last->stack_ptr = malloc(THREAD_MIN_STACK);
	if (last->stack_ptr == NULL)
	{
		free(last);
		interrupts_set(last_state);
		return THREAD_NOMEMORY;
	}

	getcontext(&last->mycontext);

	last->mycontext.uc_mcontext.gregs[REG_RSP] = (long long int)last->stack_ptr + THREAD_MIN_STACK - 8;

	last->mycontext.uc_mcontext.gregs[REG_RDI] = (long long int)fn;
	last->mycontext.uc_mcontext.gregs[REG_RSI] = (long long int)parg;
	last->mycontext.uc_mcontext.gregs[REG_RIP] = (long long int)&thread_stub;

	existing_thread[last->threadID] = true;

	int ret = last->threadID;
	interrupts_set(last_state);
	return ret;
}

Tid thread_yield(Tid want_tid)
{
	int last_state = interrupts_off();
	//TBD();
	// Free exit queue
	struct exit_queue *temp;
	while (exit_queue_head != NULL)
	{
		free(exit_queue_head->stack_ptr);
		temp = exit_queue_head;
		exit_queue_head = exit_queue_head->next;
		temp->next = NULL;
		free(temp);
	}

	if (want_tid <= -8 || want_tid >= THREAD_MAX_THREADS)
	{
		interrupts_set(last_state);
		return THREAD_INVALID;
	}

	if (want_tid == THREAD_SELF || want_tid == thread_queue_head->threadID)
	{

		volatile bool context_is_called = false;
		getcontext(&thread_queue_head->mycontext);
		if (context_is_called)
		{
			interrupts_set(last_state);
			return thread_queue_head->threadID;
		}
		context_is_called = true;
		setcontext(&thread_queue_head->mycontext);

		interrupts_set(last_state);
		return thread_queue_head->threadID;
	}

	if (want_tid == THREAD_ANY)
	{

		if (threads_count <= 1) 
		{
			interrupts_set(last_state);
			return THREAD_NONE;
		}

		volatile bool context_is_called = false;
		getcontext(&thread_queue_head->mycontext);
		if (context_is_called)
		{
			interrupts_set(last_state);
			return thread_queue_head->threadID;
		}
		push_to_back();
		context_is_called = true;
		//print_queue();
		setcontext(&thread_queue_head->mycontext);

		int ret = thread_queue_head->threadID;
		interrupts_set(last_state);
		return ret;
	}

	if (want_tid < 0 || want_tid >= THREAD_MAX_THREADS)
	{
		interrupts_set(last_state);
		return THREAD_INVALID;
	}

	if (existing_thread[want_tid] == false)
	{
		interrupts_set(last_state);
		return THREAD_INVALID;
	}

	if (threads_count == 1)
	{
		interrupts_set(last_state);
		return THREAD_NONE;
	}

	volatile bool context_is_called = false;
	getcontext(&thread_queue_head->mycontext);
	if (context_is_called)
	{
		interrupts_set(last_state);
		return want_tid;
	}
	push_to_desired(want_tid);
	context_is_called = true;
	//print_queue();
	setcontext(&thread_queue_head->mycontext);

	interrupts_set(last_state);
	return want_tid;
}

void thread_exit()
{
	//TBD();
	int last_state = interrupts_off();

	if (threads_count == 1)
	{
		struct exit_queue *temp;
		while (exit_queue_head != NULL)
		{
			free(exit_queue_head->stack_ptr);
			temp = exit_queue_head;
			exit_queue_head = exit_queue_head->next;
			temp->next = NULL;
			free(temp);
		}
		free(thread_queue_head->stack_ptr);
		free(thread_queue_head);

		interrupts_set(last_state);
		exit(0);
	}

	insert_to_exit_queue(thread_queue_head->threadID);

	// idk if this should be here
	interrupts_set(last_state);
	setcontext(&thread_queue_head->mycontext);
}

Tid thread_kill(Tid tid)
{
	int last_state = interrupts_off();

	if (tid <= 0 || tid >= THREAD_MAX_THREADS)
	{
		interrupts_set(last_state);
		return THREAD_INVALID;
	}
	if (tid == thread_queue_head->threadID || existing_thread[tid] == false)
	{
		interrupts_set(last_state);
		return THREAD_INVALID;
	}

	// if(threads_count == 1) {
	// 	thread_queue_head = kill->next;
	// }

	struct exit_queue *insert_exit_queue = insert_to_exit_queue(tid);

	interrupts_set(last_state);
	return insert_exit_queue->threadID;
}

/*******************************************************************
 * Important: The rest of the code should be implemented in Lab 3. *
 *******************************************************************/

/* make sure to fill the wait_queue structure defined above */
struct wait_queue *
wait_queue_create()
{
	int last_state = interrupts_off();
	
	struct wait_queue *wq;
	wq = malloc(sizeof(struct wait_queue));
	assert(wq);

	interrupts_set(last_state);
	return wq;
}

void wait_queue_destroy(struct wait_queue *wq)
{
	//TBD();
	int last_state = interrupts_off();
		
	if(wq->head != NULL)
		thread_wakeup(wq, 1);
	free(wq);
	interrupts_set(last_state);
}

Tid thread_sleep(struct wait_queue *queue)
{
	//TBD();
	int last_state = interrupts_off();

	if(queue == NULL)
	{
		interrupts_set(last_state);
		return THREAD_INVALID;
	}

	struct thread *sleep = thread_queue_head;
	printf("%d\n", thread_queue_head->threadID);
	//free(thread_queue_head);
	thread_queue_head = thread_queue_head->next;
	sleep->next = NULL;
	if(thread_queue_head == NULL){
		interrupts_set(last_state);
		return THREAD_NONE;
	}
	// struct wait_queue *insert_wait = queue;
	//struct wait_queue *prev = NULL;

	

	if(queue->head == NULL)
	{
		// queue = malloc(sizeof(struct wait_queue));
		// queue->mycontext = sleep->mycontext;
		// queue->stack_ptr = sleep->stack_ptr;
		// queue->threadID = sleep->threadID;
		// queue->next = NULL;
		// insert_wait = queue;
		// free(sleep);
		queue->head = sleep;
	}
	else
	{
		struct thread *temp = queue->head;
		while(temp->next != NULL)
		{
			temp = temp->next;
		}

		// insert_wait->next = malloc(sizeof(struct wait_queue));
		// insert_wait->next->mycontext = sleep->mycontext;
		// insert_wait->next->stack_ptr = sleep->stack_ptr;
		// insert_wait->next->threadID = sleep->threadID;
		// insert_wait->next->next = NULL;
		// insert_wait = insert_wait->next;
		// free(sleep);
		temp->next = sleep;
	}

	bool context_is_called = false;
	
	int rtnid;

	getcontext(&sleep->mycontext);
	if(context_is_called == false)
	{
		context_is_called = true;
		rtnid = thread_queue_head->threadID;
		setcontext(&thread_queue_head->mycontext);
	}

	

	interrupts_set(last_state);
	return rtnid;
}

/* when the 'all' parameter is 1, wakeup all threads waiting in the queue.
 * returns whether a thread was woken up on not. */
int thread_wakeup(struct wait_queue *queue, int all)
{
	//TBD();
	int last_state = interrupts_off();
	if(queue == NULL)
	{
		interrupts_set(last_state);
		return 0;
	}
	if(queue->head == NULL)
	{
		interrupts_set(last_state);
		return 0;
	}
	if(all == 1)
	{
		// int threads_awaken = 0;
		// while(queue != NULL)
		// {
		// 	struct wait_queue *wake = queue;
		// 	queue = queue->next;

		// 	struct thread *insert_ready = thread_queue_head;
		// 	if(insert_ready == NULL)
		// 	{
		// 		insert_ready = malloc(sizeof(struct thread));
		// 		insert_ready->mycontext = wake->mycontext;
		// 		insert_ready->stack_ptr = wake->stack_ptr;
		// 		insert_ready->threadID = wake->threadID;
		// 		insert_ready->next = NULL;		
		// 		free(wake);
		// 	}
		// 	else
		// 	{
		// 		while(insert_ready->next != NULL)
		// 		{
		// 			insert_ready = insert_ready->next;
		// 		}
				
		// 		insert_ready->next = malloc(sizeof(struct thread));
		// 		insert_ready->next->mycontext = wake->mycontext;
		// 		insert_ready->next->stack_ptr = wake->stack_ptr;
		// 		insert_ready->next->threadID = wake->threadID;
		// 		insert_ready->next->next = NULL;		
		// 		free(wake);
		// 	}
		// threads_awaken++;
		// }
		// interrupts_set(last_state);
		// return threads_awaken;
	}
	else
	{

		struct thread *temp = queue->head;
		queue->head = queue->head->next;
		temp->next = NULL;

		// printf("BEFORE-----------------------");
		// //print_queue(queue);
		// struct wait_queue *wake = queue;
		// queue = queue->next;

		struct thread *insert_ready = thread_queue_head;
		if(insert_ready == NULL)
		{
			// thread_queue_head = malloc(sizeof(struct thread));
			// thread_queue_head->mycontext = wake->mycontext;
			// thread_queue_head->stack_ptr = wake->stack_ptr;
			// thread_queue_head->threadID = wake->threadID;
			// thread_queue_head->next = NULL;		
			// free(wake);
			thread_queue_head = temp;
			thread_queue_head->next = NULL;
		}
		else
		{
			while(insert_ready->next != NULL)
			{
				insert_ready = insert_ready->next;
			}
			
			// insert_ready->next = malloc(sizeof(struct thread));
			// insert_ready->next->mycontext = wake->mycontext;
			// insert_ready->next->stack_ptr = wake->stack_ptr;
			// insert_ready->next->threadID = wake->threadID;
			// insert_ready->next->next = NULL;		
			// free(wake);
			insert_ready->next = temp;
			//printf("AFTER-----------------------");
			//print_queue(queue);

		}

		interrupts_set(last_state);
		return 1;
	}
	

	interrupts_set(last_state);
	return 0;
}

/* suspend current thread until Thread tid exits */
Tid thread_wait(Tid tid)
{
	TBD();
	return 0;
}

struct lock
{
	/* ... Fill this in ... */
};

struct lock *
lock_create()
{
	struct lock *lock;

	lock = malloc(sizeof(struct lock));
	assert(lock);

	TBD();

	return lock;
}

void lock_destroy(struct lock *lock)
{
	assert(lock != NULL);

	TBD();

	free(lock);
}

void lock_acquire(struct lock *lock)
{
	assert(lock != NULL);

	TBD();
}

void lock_release(struct lock *lock)
{
	assert(lock != NULL);

	TBD();
}

struct cv
{
	/* ... Fill this in ... */
};

struct cv *
cv_create()
{
	struct cv *cv;

	cv = malloc(sizeof(struct cv));
	assert(cv);

	TBD();

	return cv;
}

void cv_destroy(struct cv *cv)
{
	assert(cv != NULL);

	TBD();

	free(cv);
}

void cv_wait(struct cv *cv, struct lock *lock)
{
	assert(cv != NULL);
	assert(lock != NULL);

	TBD();
}

void cv_signal(struct cv *cv, struct lock *lock)
{
	assert(cv != NULL);
	assert(lock != NULL);

	TBD();
}

void cv_broadcast(struct cv *cv, struct lock *lock)
{
	assert(cv != NULL);
	assert(lock != NULL);

	TBD();
}