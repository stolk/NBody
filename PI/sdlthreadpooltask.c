#include "sdlthreadpooltask.h"

#include "SDL_thread.h"
#include "SDL_mutex.h"

#include <assert.h>


//=========
// Argument
//=========

argument_t NO_ARGUMENT = { NULL, NO_DELETE, NULL };

argument_t make_argument(void *arg, memory_t del_arg, void *result)
{
	argument_t argument = { arg, del_arg, result };
	return argument;
}



//=====
// Task
//=====

//The source for the unique id's
taskID taskIDSource = 1;
//pthread_mutex IDSource_lock = PTHREAD_MUTEX_INITIALIZER;	//Provides synchronized access to the taskIDSource
SDL_mutex* IDSource_lock = 0;


task_t *task_creat(work_function work, argument_t argument, priority_t priority)
{
	//Check if valid work function
	if(work == NULL) {
		fprintf(stderr, "Invalid work function in task_create().\n");
		return NULL;
	}

	//Allocate memory
	task_t *task = (task_t*) malloc(sizeof(task_t));
	assert( task );

	if ( !IDSource_lock )
	{
		IDSource_lock = SDL_CreateMutex();	//Provides synchronized access to the taskIDSource
		assert( IDSource_lock );
	}

	//Generate an unique id
	SDL_LockMutex(IDSource_lock);
	task->ID = taskIDSource++;
	SDL_UnlockMutex(IDSource_lock);

	//Initialize parameters
	task->work = work;
	task->argument = argument;
	task->priority = priority;
	task->status = NOT_EXECUTED;
	task->next = NULL;

	return task;
}

int task_exec(task_t *task)
{
	//Check if already executed
	if(task->status == EXECUTED)
		return 0;

	//Execute task
	task->work(&(task->argument));

	//Free argument memory
	if(task->argument.del_arg == DELETE)
		free(task->argument.arg);

	//Change the status
	task->status = EXECUTED;

	return 1;
}

void task_free(task_t *task)
{
	//If not executed, free the argument memory
	if(task->status == NOT_EXECUTED && task->argument.del_arg == DELETE) {
		free(task->argument.arg);
	}

	//Free task memory
	free(task);
}



//==========
// Task list
//==========

tasklist_t *tasklist_create()
{
	//Allocate memory
	tasklist_t *tasklist = (tasklist_t*) malloc(sizeof(tasklist_t));

	//Check if allocated
	if(!tasklist) {
		fprintf(stderr, "Allocation failed in tasklist_create().\n");
		return NULL;
	}

	//Initialize parameters
	tasklist->head = NULL;
	tasklist->n_tasks = 0;

	return tasklist;
}

int tasklist_insert(tasklist_t *tasklist, task_t *task)
{
	//Make sure that the task is not NULL
	if(!task)
		return 0;

	//Don't add already executed tasks
	if(task->status == EXECUTED)
		return 0;

	//If the function gets here, the task will be added, so increment the number of tasks
	//in the list
	tasklist->n_tasks++;

	//If it's the first element of the list
	if(tasklist->head == NULL) {
		tasklist->head = task;
	} else {
		//Iterate through the list to find the last task with a priority <= task->priority
		task_t *it;
		for(it = tasklist->head; it->next != NULL && task->priority <= it->next->priority; it = it->next);

		//Check if the task should be the new head
		if(it == tasklist->head && task->priority > it->priority) {
			task->next = tasklist->head;
			tasklist->head = task;
		} else {
			//Add at the current position
			task->next = it->next;
			it->next = task;
		}
	}

	return 1;
}

task_t *tasklist_get(tasklist_t *tasklist, taskID ID)
{
	//Iterate through the list
	task_t *it;
	for(it = tasklist->head; it != NULL; it = it->next)
		if(it->ID == ID)
			return it;

	//Not found
	return NULL;
}

task_t *tasklist_pop(tasklist_t *tasklist)
{
	//If empty
	if(tasklist->n_tasks == 0)
		return NULL;

	//Decrement the number of tasks
	tasklist->n_tasks--;

	//Get the head and set the next pointer to NULL
	task_t *head = tasklist->head;
	tasklist->head = head->next;
	head->next = NULL;

	return head;
}

task_t *tasklist_remove(tasklist_t *tasklist, taskID ID)
{
	//If no tasks, return NULL
	if(tasklist->n_tasks == 0)
		return NULL;

	//Check if it's the head
	if(tasklist->head->ID == ID)
		return tasklist_pop(tasklist);

	//The task that will be returned
	task_t *result = NULL;	//Initialize to NULL in case no task was found

	//Iterate through the tasks and check the IDs
	task_t *it;
	for(it = tasklist->head; it->next != NULL; it = it->next)
		if(it->next->ID == ID) { //If found
			//Get the result
			result = it->next;

			//Re-link list
			it->next = result->next;
			result->next = NULL;

			//Decrement the number of tasks
			tasklist->n_tasks--;

			break;
		}

	return result;
}

void tasklist_clear(tasklist_t *tasklist)
{
	//Pop and free
	task_t *it;
	while((it=tasklist_pop(tasklist)) != NULL)
		task_free(it);
}

void tasklist_free(tasklist_t *tasklist)
{
	//Free all the tasks
	tasklist_clear(tasklist);

	//Free memory used by the list structure
	free(tasklist);
}
