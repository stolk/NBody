#include "sdlthreadpool.h"
#if defined(linux)
#	include "threadtracer.h"
#endif
#include <assert.h>

#ifndef REF
#define REF(x) &(x)
#endif

int threadpool_worker(void *tp)
{
	//Get the thread pool pointer
	threadpool_t *threadpool = (threadpool_t*) tp;
	assert( threadpool );

	//The task to be executed
	task_t *task=0;

#if defined(linux)
	tt_signin( -1, "worker" );
#endif

	for( ; ; ) {
		//Aquire lock
		SDL_LockMutex( threadpool->mutex );

		//Wait for an available task
		while(!threadpool->stop && !(task = tasklist_pop(threadpool->tasks)))
			SDL_CondWait( threadpool->new_work, threadpool->mutex );

		//If the thread got a signal to stop
		if(threadpool->stop) {
			SDL_UnlockMutex( threadpool->mutex );
			break;
		}

		//Add task to the executing list and unlock the mutex
		tasklist_insert(threadpool->executing, task);
		SDL_UnlockMutex( threadpool->mutex );

		//Execute the task
		task_exec(task);

		//Aquire lock
		SDL_LockMutex( threadpool->mutex );

		//Remove from executing list and free task
		tasklist_remove(threadpool->executing, task->ID);
		task_free(task);

		//Notify that a task was done
		SDL_CondBroadcast( threadpool->task_done );

		//Unlock
		SDL_UnlockMutex( threadpool->mutex );
	}

	//pthread_exit(NULL);
	return 0;	//Stop bugging me eclipse
}


threadpool_t *threadpool_create(int n_thread)
{
	//Allocate memory for the thread pool struct
	threadpool_t *threadpool = (threadpool_t*) malloc(sizeof(threadpool_t));
	assert( threadpool );

	//Initialize parameters
	threadpool->threads = (SDL_Thread**) malloc(sizeof(SDL_Thread*) * n_thread);
	threadpool->n_threads = n_thread;

	//pthread_mutex_init(REF(threadpool->mutex), NULL);
	threadpool->mutex = SDL_CreateMutex();
	threadpool->stop = 0;
	threadpool->tasks = tasklist_create();
	threadpool->executing = tasklist_create();
	//pthread_cond_init(REF(threadpool->new_work), NULL);
	//pthread_cond_init(REF(threadpool->task_done), NULL);
	threadpool->new_work = SDL_CreateCond();
	threadpool->task_done = SDL_CreateCond();

	//Start threads
	int i;
	for(i = 0; i < n_thread; i++)
	{
#if 1
		threadpool->threads[ i ] = SDL_CreateThread
		(
			threadpool_worker,
			"threadpool_worker",
			(void*) threadpool
		);
#else
		int error_code;
		if((error_code = pthread_create(REF(threadpool->threads[i]), NULL, threadpool_worker, (void*) threadpool)) != 0) {
			fprintf(stderr, "Error starting worker thread(error code %d).\n", error_code);
			exit(-1);
		}
#endif
	}

	return threadpool;
}

taskID threadpool_addTask(threadpool_t *threadpool, task_t *task)
{
	assert(threadpool);
	//Acquire lock
	//pthread_mutex_lock(REF(threadpool->mutex));
	SDL_LockMutex( threadpool->mutex );

	//The return value
	taskID ID = 0;

	//Add to tasks
	if(tasklist_insert(threadpool->tasks, task)) {
		//If added notify a worker thread
		//pthread_cond_signal(REF(threadpool->new_work));
		SDL_CondSignal( threadpool->new_work );
		ID = task->ID;
	}

	//Unlock and return
	//pthread_mutex_unlock(REF(threadpool->mutex));
	SDL_UnlockMutex( threadpool->mutex );
	return ID;
}

taskID threadpool_add(threadpool_t *threadpool, work_function work, argument_t argument, priority_t priority)
{
	//Create task
	task_t *task = task_creat(work, argument, priority);

	//Add to threadpool
	return threadpool_addTask(threadpool, task);
}

void threadpool_wait(threadpool_t *threadpool)
{
	//Acquire lock
	//pthread_mutex_lock(REF(threadpool->mutex));
	SDL_LockMutex( threadpool->mutex );

	//Wait until there are no more tasks available or executing
	while(threadpool->tasks->n_tasks || threadpool->executing->n_tasks)
		//pthread_cond_wait(REF(threadpool->task_done), REF(threadpool->mutex));
		SDL_CondWait( threadpool->task_done, threadpool->mutex );

	//Unlock
	//pthread_mutex_unlock(REF(threadpool->mutex));
	SDL_UnlockMutex( threadpool->mutex );
}

void threadpool_waitTask(threadpool_t *threadpool, taskID ID)
{
	//Acquire lock
	//pthread_mutex_lock(REF(threadpool->mutex));
	SDL_LockMutex( threadpool->mutex );

	//Wait
	while(tasklist_get(threadpool->executing, ID) || tasklist_get(threadpool->tasks, ID)) {
		//pthread_cond_wait(REF(threadpool->task_done), REF(threadpool->mutex));
		SDL_CondWait( threadpool->task_done, threadpool->mutex );
	}

	//Unlock
	//pthread_mutex_unlock(REF(threadpool->mutex));
	SDL_UnlockMutex( threadpool->mutex );
}

int threadpool_isDone(threadpool_t *threadpool, taskID ID)
{
	//Acquire lock
	//pthread_mutex_lock(REF(threadpool->mutex));
	SDL_LockMutex( threadpool->mutex );

	//Check
	int isDone = 1;
	if(tasklist_get(threadpool->executing, ID) || tasklist_get(threadpool->tasks, ID))
		isDone = 0;

	//Unlock
	//pthread_mutex_unlock(REF(threadpool->mutex));
	SDL_UnlockMutex( threadpool->mutex );

	return isDone;
}

void threadpool_free(threadpool_t *threadpool)
{
	//Wait for tasks to be executed
	threadpool_wait(threadpool);

	//Signal threads to stop
	//pthread_mutex_lock(REF(threadpool->mutex));
	SDL_LockMutex( threadpool->mutex );
	threadpool->stop = 1;
	//pthread_cond_broadcast(REF(threadpool->new_work));
	SDL_CondBroadcast( threadpool->new_work );
	//pthread_mutex_unlock(REF(threadpool->mutex));
	SDL_UnlockMutex( threadpool->mutex );

	//Join threads
	int i;
	for(i = 0; i < threadpool->n_threads; i++)
	{
		int rv;
		SDL_WaitThread( threadpool->threads[ i ], &rv );
		//pthread_join(threadpool->threads[i], NULL);
	}

	//Free the memory
	free(threadpool->threads);
	//pthread_mutex_destroy(REF(threadpool->mutex));
	SDL_DestroyMutex( threadpool->mutex );
	tasklist_free(threadpool->tasks);
	tasklist_free(threadpool->executing);
	SDL_DestroyCond( threadpool->new_work );
	SDL_DestroyCond( threadpool->task_done );
	//pthread_cond_destroy(REF(threadpool->new_work));
	//pthread_cond_destroy(REF(threadpool->task_done));
	free(threadpool);
}

void *threadpool_background(void *tp)
{
	//Get pointer to the thread pool
	threadpool_t *threadpool = (threadpool_t*) tp;

	//Free memory when done
	threadpool_free(threadpool);

	//pthread_exit(NULL);
	return NULL;	//Pls eclipse
}

#if 0
void threadpool_freeLater(threadpool_t *threadpool)
{
	//Start thread
	pthread_t background;
	if(pthread_create(REF(background), NULL, threadpool_background, (void*) threadpool)) {
		fprintf(stderr, "Error starting background thread.\n");
		exit(-1);
	}

	//Detach thread
	pthread_detach(background);
}
#endif

