/**
 * TODO:
 * 		1. Make the list more efficient(no more O(N) times).
 */
#ifndef SDLTHREADPOOLTASK_H_
#define SDLTHREADPOOLTASK_H_

#include <stdio.h>
#include <stdlib.h>
#include "SDL.h"

//===============================================
// Indicates if a pointer should be freed or not.
//===============================================
typedef enum {
	NO_DELETE,
	DO_DELETE
} memory_t;


//=======================================
// Argument for the task's work function.
//=======================================
typedef struct {
	void *arg;			//The main argument
	memory_t del_arg;	//Indicates if the arg pointer should be freed
	void *result;		//Additional pointer in case the result should be written somewhere
} argument_t;

//Used when a work function doesn't need an argument.
extern argument_t NO_ARGUMENT;

/**
 * make_argument() - Returns an initialized argument structure.
 *
 * input: @arg main argument, @del_arg tells is arg should be freed after execution,
 * @result in case the result should be written somewhere.
 * output: @return initialized argument_t structure.
 * example:
 * 		int *myArg = (int*) malloc(sizeof(int));
 * 		*myArg = 10;
 * 		int myResult;
 * 		argument_t myArgument = make_argument((void*) myArg, DELETE, (void*) &myResult);
 */
argument_t make_argument(
	void *arg,
	memory_t del_arg,
	void *result
);


//=======================================
// The function that does the task's work
//=======================================
typedef void (*work_function)(argument_t *);



//====================================================
// Every task can have one of the following priorities
//====================================================
typedef enum {
	LOW,
	MEDIUM,
	HIGH,
	DEFAULT = MEDIUM
} priority_t;

//==========================================
// Indicates if the task was executed or not
//==========================================
typedef enum {
	EXECUTED,
	NOT_EXECUTED
} status_t;


//=======================
// Unique task identifier
//=======================
typedef int32_t taskID;



//========================
// The task data structure
//========================
typedef struct task {
	taskID ID;				//The task id
	work_function work;		//The work function
	argument_t argument;	//The argument
	priority_t priority;	//Task priority
	status_t status;		//Status
	struct task *next;		//Used to implement the list
} task_t;

/**
 * task_create() - Allocates memory and initializes a task structure.
 *
 * input: @work work function, @argument argument for the work function, @priority priority of the task.
 * output: @return the newly created task.
 * errors: @return NULL if allocation failed or NULL work function.
 * note:
 * 		1. the return task will have a unique ID.
 * 		2. it's thread safe.
 * example:
 * 		task_t *myTask = task_create(myWork, make_argument(myArg, DELETE, myResult), HIGH);
 * 		//Free memory
 */
task_t *task_creat(
	work_function work,
	argument_t argument,
	priority_t priority
);

/**
 * task_exec() - Executes the given task.
 *
 * input: @task the task to be executed.
 * output: @return 1 if executed, 0 otherwise.
 * note:
 * 		1. if task.argument.del_arg == DELETE, the task.argument.arg will be deleted after execution.
 * 		2. a task cannot be executed more than once.
 * example:
 *		task_t *myTask = task_create(myWork, make_argument(myArg, DELETE, myResult), HIGH);
 *		task_exec(myTask);
 *		//Free task memory
 */
int task_exec(
	task_t *task
);

/**
 * task_free() - Frees all the memory used by the task.
 *
 * input: @task the task.
 * note: if the task is not executed, it will also free the argument memory(assuming del_arg == DELETE).
 * example:
 * 		task_t *myTask = task_create(myWork, make_argument(myArg, DELETE, myResult), HIGH);
 * 		task_exec(myTask);
 * 		task_free(myTask);
 */
void task_free(
	task_t *task
);



//=======================================
// List of tasks, ordered by the priority
//=======================================
typedef struct {
	task_t *head;		//The head of the list
	int32_t n_tasks;	//The number of tasks contained in the list
} tasklist_t;

/**
 * tasklist_create() - Allocates memory and initializes a list with no tasks.
 *
 * output: @return the newly create task list.
 * errors: @return NULL if allocation failed.
 * example:
 * 		tasklist_t *tasklist = tasklist_create();
 * 		//Free memory
 */
tasklist_t *tasklist_create();

/**
 * tasklist_insert() - Inserts a task in the task list.
 *
 * input: @tasklist the task list, @task the task.
 * output: @return 1 if inserted, 0 otherwise.
 * note: it doesn't executed tasks.
 * example:
 * 		tasklist_t *tasklist = tasklist_create();
 * 		task_t *task = ...;
 * 		if(tasklist_insert(tasklist, task))
 * 			//Inserted
 * 		else
 * 			//Not inserted
 */
int tasklist_insert(
	tasklist_t *tasklist,
	task_t *task
);

/**
 * tasklist_hasTask() - Retrieves a task from the task list.
 *
 * input: @tasklist the task list, @ID the task's id.
 * output: @return returns the task if the list contains it, or NULL otherwise.
 */
task_t *tasklist_get(
	tasklist_t *tasklist,
	taskID ID
);

/**
 * tasklist_pop() - Retrieves the first added task from the tasks with the highest priority.
 *
 * input: @tasklist the task list.
 * output: @return high priority task, or NULL is empty.
 * example:
 * 		//Insert into tasklist: medium, high1, high2, low(in this order)
 * 		tasklist_pop(tasklist) == high1
 * 		tasklist_pop(tasklist) == high2
 * 		tasklist_pop(tasklist) == medium
 * 		tasklist_pop(tasklist) == low
 */
task_t *tasklist_pop(
	tasklist_t *tasklist
);

/**
 * tasklist_remove() - Removes the task with the given ID.
 *
 * input: @tasklist the task list, @ID the task's ID.
 * output: @return returns the removed task or NULL is no such task.
 */
task_t *tasklist_remove(
	tasklist_t *tasklist,
	taskID ID
);

/**
 * tasklist_clear() - Removes and frees all the tasks.
 *
 * input: @tasklist the task list.
 */
void tasklist_clear(
	tasklist_t *tasklist
);

/**
 * tasklist_free() - Frees all the memory used by the task list(including freeing all the tasks).
 *
 * input: @tasklist the task list.
 */
void tasklist_free(
	tasklist_t *tasklist
);

/**
 * task_h_runTests() - Clear list and free list structure memory.
 */
void task_h_runTests();

#endif /* TASK_H_ */
