/**
 * TODO:
 * 		1. Performance test regarding using one or 2 mutexes.
 */
#ifndef SDLTHREADPOOL_H_
#define SDLTHREADPOOL_H_

#include "sdlthreadpooltask.h"

#include "SDL_thread.h"
#include "SDL_mutex.h"


//===========================
// The thread pool structure.
//===========================
typedef struct {
	SDL_Thread** threads;			//Array with threads
	int n_threads;				//The number of threads

	SDL_mutex* mutex;		//The mutex that protects all the data

	int stop;					//Tells the threads if they should stop or not

	tasklist_t *tasks;			//The tasks in the queue
	tasklist_t *executing;		//The tasks that are being executed

	SDL_cond* new_work;	//Condition variable used to notify the workers if there is
								//new work available, or if they should stop
	SDL_cond* task_done;	//Notify when a worker has finished a task
} threadpool_t;


/**
 * threadpool_worker() - Worker thread.
 * input: @tp the thread pool.
 * output: NULL.
 * note: ...TODO
 */
int threadpool_worker( void *tp);

/**
 * threadpool_create() - Allocate memory, initialize variables and start threads.
 *
 * input: @n_threads the number of threads assigned to the thread pool.
 * output: @return the newly created thread pool.
 * note: the threads start when the right away.
 */
threadpool_t *threadpool_create(
	int n_thread
);

/**
 * threadpool_addTask() - Adds a task to the thread pool.
 *
 * input: @threadpool the thread pool, @task the task.
 * output: @return the task id.
 * errors: @return 0 if task no added.
 */
taskID threadpool_addTask(
	threadpool_t *threadpool,
	task_t *task
);

/**
 * threadpool_add() - Created and adds a task to the thread pool.
 *
 * input: @threadpool the thread pool, @work the work function, @argument the argument, @priority task priority.
 * output: @return the task id.
 * errors: @return -1 if task no added.
 */
taskID threadpool_add(
	threadpool_t *threadpool,
	work_function work,
	argument_t argument,
	priority_t priority
);

/**
 * threadpool_wait() - Wait for all the current tasks to be executed.
 *
 * input: @threadpool the threadpool.
 * note: this function will block until all the tasks are executed.
 */
void threadpool_wait(
	threadpool_t *threadpool
);

/**
 * threadpool_waitTask() - Waits for a task to me executed.
 *
 * input: @threadpool the thread pool, @ID the task's id.
 */
void threadpool_waitTask(
	threadpool_t *threadpool,
	taskID ID
);

/**
 * threadpool_isDone() - Checks if a taks has done executing.
 *
 * input: @threadpool the thread pool, @ID the task's id.
 * output: @return 1 if done, 0 otherwise.
 * note: it will return 1 even if the task was never contained in the thread pool.
 */
int threadpool_isDone(
	threadpool_t *threadpool,
	taskID ID
);

/**
 * threadpool_free() - Waits for all the tasks to be done and then frees the memory.
 *
 * input: @threadpool the thread pool.
 */
void threadpool_free(
	threadpool_t *threadpool
);

/**
 * threadpool_freeLater() - Automatically free the thread pool when all the tasks are done.
 *
 * input: @threadpool the thread pool.
 * note:
 * 		1. non-blocking.
 * 		2. after a call to this function the threadpool pointer should not be accessed anymore.
 * 		3. it starts a new background thread that will take care of the memory management.
 */
void threadpool_freeLater(
	threadpool_t *threadpool
);

#endif /* THREADPOOL_H_ */
