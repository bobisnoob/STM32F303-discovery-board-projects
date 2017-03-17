/*
* File Name	 	 : multitasking.c
* PROJECT		 : INFO8110 Assignment #2
* PROGRAMMERS	 : Abdel Alkuor
* FIRST VERSION  : 2017-03-13
* Description    : This program demonstartes multi-tasking operating systems
				   uses linked-list to grow the add tasks dynamically.the user 
				   can exercise one of the following function in the minicom
				   terminal:
				 * add new function            : TaskAdd (function name), (unique ID)
       	         * delete existing fucntion    : killtask (task ID)
				 * change task ID              : changeTaskID (new ID), (old ID)
			     * print current task IDs      : printTaskIDs   	
				 * run taskswitchers		   : run
				 * To delete all existing tasks: TaskKillAll
				 * To initialize functions list: initfunctions
				 * To display function names   : displayfunctions
 						
**/	


#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include "stm32f3xx_hal.h"
#include "common.h"
#include <string.h>
#include "LCD1602A.h"
#include "stm32f3_discovery.h"


#define EXSITED_FUCNTION_SIZE 5



// struct definitions
typedef struct Task_s
{
	void (*f)(void *data);
	void *data;
	int32_t taskID;
	struct Task_s *nextTask;
}Task_t;

typedef struct 
{
	void(*f)(void *data);
	char * Name;
}functionsPool;

//global variables
extern volatile uint8_t stopFlag;
extern volatile uint8_t roletteFlag;
extern TIM_HandleTypeDef htim16;
volatile uint32_t currentTask;
uint32_t tasks_size ;
Task_t *headOfTasks ; 
Task_t *tempNextTask ;
int8_t nextLed = 0;
int8_t prevLed = 0;
char *string ="Hello World!";
functionsPool availablefucntions[EXSITED_FUCNTION_SIZE];

//functions prototypes
int checkID(Task_t* current, Task_t* new);
int buildFunctions(void(*f)(void *data), char *funcName);
void changeTaskID(int mode);
void printTaskIDs(int mode);
void TaskKillAll(int mode);
void print(void *);
void TaskAdd(int mode);
void TaskKill(int mode);
int32_t TaskSwitcher(void);
void TaskCurrent(int mode);
Task_t* TaskNext(void);


/* FUNCTION      : roletteWheel
 * DESCRIPTION   : This function runs the led on the stm32f303
				   discoveryboard in rolette wheel behaviour  
 * PARAMETERS    : void *data
 * RETURNS       : NULL
*/
void roletteWheel(void *data)
{
	HAL_TIM_Base_Stop_IT(&htim16);
	if(roletteFlag ==1)
	{
		print(data);
		if(prevLed==nextLed)
		{
			BSP_LED_Toggle(prevLed);
			nextLed++;
		}
		else
		{
			BSP_LED_Toggle(prevLed);
			BSP_LED_Toggle(nextLed);
			prevLed = nextLed;
			nextLed++;
		}
		if(nextLed==8) nextLed = 0;
		roletteFlag =0;
	}
	HAL_TIM_Base_Start_IT(&htim16);

}
/* FUNCTION      : LCD1
 * DESCRIPTION   : This function prints "Hello World"
				   the first LCD row  
 * PARAMETERS    : void *data
 * RETURNS       : NULL
*/

void LCD1(void *data)
{	
	print(data);
	LCD1602A_WriteLineOne(0,string);
	HAL_Delay(200);
	clearLCD();

}

/* FUNCTION      : LCD2
 * DESCRIPTION   : This function prints "Hello World"
				   the second LCD row  
 * PARAMETERS    : void *data
 * RETURNS       : NULL
*/

void LCD2(void *data)
{
	print(data);
	LCD1602A_WriteLineTwo(0,string);
	HAL_Delay(200);
	clearLCD();
}

/* FUNCTION      : initfunctions
 * DESCRIPTION   : This function allows the user to add
     			   the created functions to the function
				   pool by using "buildFunctions" function
 * PARAMETERS    : int mode
 * RETURNS       : NULL
*/

void initfunctions(int mode)
{

	buildFunctions(LCD1,"LCD1");
	buildFunctions(LCD2,"LCD2");
	buildFunctions(roletteWheel,"roletteWheel");	
}
ADD_CMD("initfunctions",initfunctions,"      build functions")

/* FUNCTION      : printfunctionnames
 * DESCRIPTION   : This function display the function
	               names if any is available
 * PARAMETERS    : int mode
 * RETURNS       : NULL
*/
void printfunctionnames(int mode)
{

	for(int i= 0 ; i<EXSITED_FUCNTION_SIZE; i++)
	{	
		if(i==0 && availablefucntions[i].Name==NULL)
		{
			printf("Make sure to initialize functions\n");
			return;
		}
		else
		{	
			if(availablefucntions[i].Name!=NULL)
			printf("%s ",availablefucntions[i].Name);
		}
	}	
	printf("\n");
}
ADD_CMD("displayfunctions",printfunctionnames,"      build functions")

/* FUNCTION      : run
 * DESCRIPTION   : This function runs the TaskSwitcher
				   It is used after adding tasks to run
 * PARAMETERS    : int mode
 * RETURNS       : NULL
*/
void run (int mode)
{	
	int j =0;
	int status =0;
	while(j<50 && stopFlag)
	{
		status = TaskSwitcher();
		if(status ==-1) 
		{
			printf("There are no tasks to run!\n");
			break;
		}
		j++;
	}
	
	HAL_TIM_Base_Stop_IT(&htim16);
	stopFlag = 1;
	HAL_TIM_Base_Start_IT(&htim16);
}

ADD_CMD("run",run,"         Run TaskSwitcher")


/* FUNCTION      : TaskAdd
 * DESCRIPTION   : This function can be used to add new
				   tasks using dynamic memory alloction
				   (linked-list) 
 * PARAMETERS    : int mode
 * RETURNS       : NULL
*/

void  TaskAdd(int mode)
{
	char *functionName;
	int rc;
	int i=0;
	volatile int status = 1;
	Task_t *newTask;
	Task_t *tempTask;
	newTask = (Task_t*)malloc(sizeof(Task_t));
	rc = fetch_string_arg(&functionName);
	if(rc)
	{
		printf("No function name entered\n");
		free(newTask);
		return ;
	}
	
	while( i < EXSITED_FUCNTION_SIZE )
	{
		status = strcmp(functionName, availablefucntions[i].Name);
		if(status== 0)
		{
			break;
		}
		i++;
	}
	if(status!=0)	
	{
		printf("function name doesn't exist\n");
		free(newTask);
		return ;
	}
	rc = fetch_int32_arg(&(newTask->taskID));
	if(rc)
	{
		printf("No Task ID entered\n");
		free(newTask);
		return ;
	}
	if(newTask->taskID <0)
	{
		printf("Task ID must be bigger than -1\n");
		free(newTask);
		return ;
	}
		
	newTask->f = availablefucntions[i].f;
	newTask->data = &(newTask->taskID);
	newTask->nextTask = NULL;	
	if(headOfTasks == NULL)
	{
		headOfTasks = newTask;
		tasks_size++;
		return ;
	}
	
	tempTask = headOfTasks;
	
	while(tempTask->nextTask != NULL)
	{
		rc = checkID(tempTask,newTask);
		if(rc) return;
		tempTask = tempTask->nextTask;
	}
	rc = checkID(tempTask,newTask);
	if(rc) return;
	
	tasks_size++;	
	tempTask->nextTask = newTask;
	return;
}
ADD_CMD("TaskAdd",TaskAdd,"Enter task name and ID ")

/* FUNCTION      : TaskKill
 * DESCRIPTION   : This function can be used to remove
				   a task from the tasks pool by knowing
				   its ID
 * PARAMETERS    : int mode
 * RETURNS       : NULL
*/

void TaskKill(int mode)
{
	int32_t id;
	int rc;
	rc = fetch_int32_arg(&id);
	if(rc)
	{
		printf("No newID entered\n");
		return;
	}
	if(id<0)
	{
		printf("the entered ID must be bigger than -1\n");
	}
	Task_t *searchID=headOfTasks;
	Task_t *prevTask=NULL;
	if(searchID == NULL)
	{
		printf("Tasks list is empty\n");
		return;
	}

	while(searchID != NULL)
	{
		if(searchID->taskID == id)
		{	
			 tasks_size--;
			 currentTask = 0;	
			 break;
		}
		else
		{
			prevTask = searchID;
			searchID = searchID->nextTask;
		}
	}

	if(searchID == NULL)
	{
		printf("The entered task ID doesn't exist\n");
		return ;
	}
	else if (searchID == headOfTasks)
	{
		headOfTasks = headOfTasks->nextTask;
		free(searchID);
		printf("Task %ld is gone for good\n", id);
		return;
	}
	else
	{
		prevTask->nextTask = searchID->nextTask;
		free(searchID);
		printf("Task %ld is gone for good\n", id);
		return;
	}
}	
ADD_CMD("killtask",TaskKill,"Enter task ID to delete")

/* FUNCTION      : TaskNext
 * DESCRIPTION   : This function is used by the
				   TaskSwitcher to advance to the 
					next task in the tasks pool
 * PARAMETERS    : void
 * RETURNS       : struct pointer of Task_t type
*/

Task_t *TaskNext(void)
{
	if(currentTask==tasks_size)
	{
		 currentTask = 0;
	}
	
	if(headOfTasks == NULL) return NULL;
	if(currentTask == 0)
	{	
		tempNextTask = headOfTasks;
	}
	else
	{
		tempNextTask = tempNextTask->nextTask;
	}
	currentTask++;
	return tempNextTask;			
}

/* FUNCTION      : TaskCurrent
 * DESCRIPTION   : This function can be used
				   to examin the curret running
				   function
 * PARAMETERS    : int mode
 * RETURNS       : NULL
*/
		
void  TaskCurrent(int mode)
{	
	if(TaskNext()!= NULL)
	{
		(TaskNext()->f)(TaskNext()->data);
		return;
	}
	printf("There is no current Task to run\n");
}	
ADD_CMD("TaskCurrent",TaskCurrent,"Current running Task")


/* FUNCTION      : TaskSwitcher
 * DESCRIPTION   : This function is used to run all
					tasks in the current tasks pool
 * PARAMETERS    : void
 * RETURNS       : int32_t 
*/	
int32_t TaskSwitcher(void)
{
	 Task_t *currentTask = TaskNext();
	if(currentTask == NULL) return -1;
	(currentTask->f)(currentTask->data);
	return 1;
}	

/* FUNCTION      : print
 * DESCRIPTION   : This function is used to print
				   the current task ID
 * PARAMETERS    : void * taskNumber
 * RETURNS       : NULL
*/	
void print(void *taskNumber)
{
	printf("Task %u is running\n",*(uint16_t*)taskNumber);
}

/* FUNCTION      : TaskKillAll
 * DESCRIPTION   : This function deletes all the
				   current tasks in the tasks pool
 * PARAMETERS    : int mode
 * RETURNS       : NULL
*/	

void TaskKillAll(int mode)
{	
	Task_t *freeTask;
	if(headOfTasks == NULL) return ;
	
	while(headOfTasks !=NULL)
	{
		freeTask = headOfTasks;
		headOfTasks = headOfTasks->nextTask;
		free(freeTask);
	}
	currentTask=0;
	tasks_size =0;
	printf("Slaughter has just happened!!\n");
}
ADD_CMD("Killalltasks",TaskKillAll,"Delete all tasks")

/* FUNCTION      : printTaskID
 * DESCRIPTION   : This function prints out all
				   the current available tasks
 * PARAMETERS    : int mode
 * RETURNS       : NULL
*/	
void printTaskIDs(int mode)
{
	Task_t *tempTask = headOfTasks;
	if(headOfTasks == NULL) return;
	printf("Current Task IDs: ");
	
	while(tempTask != NULL)
	{
		printf("%ld ",tempTask->taskID);
		tempTask = tempTask->nextTask;
	}
	printf("\n");
}
ADD_CMD("printtaskIDs",printTaskIDs,"print out available task IDs")


/* FUNCTION      : changeTaskID
 * DESCRIPTION   : This function allows the user
				   to change task ID to new one
 * PARAMETERS    : int mode
 * RETURNS       : NULL
*/			
void changeTaskID(int mode)
{
	int rc;
	int32_t newID, oldID;
	rc = fetch_int32_arg(&newID);
	if(rc)
	{
		printf("No newID entered\n");
		return;
	}
	if(newID<0)
	{
		printf("ID must be bigger than -1\n");
		return;
	}	
	rc = fetch_int32_arg(&oldID);	
	if(rc)
	{
		printf("No oldID entered\n");
		return;
	}
	if(oldID<0)
	{
		printf("ID must be bigger than -1\n");
		return;
	}	
	
	Task_t *searchID=headOfTasks;
	if(searchID == NULL)
	{
		printf("Task list is empty\n");
		return ;// empty tasks list
	}

	while(searchID != NULL) // check if the new id already exists
	{
		if(searchID->taskID == newID)
		{
			printf("The new entered ID is already existed\n");
			return;
		}
		else
		{
			searchID = searchID->nextTask;
		}
	}

	searchID = headOfTasks;
	while(searchID != NULL) //change old id with new id
	{
		if(searchID->taskID == oldID)
		{
			 searchID->taskID = newID;	
			 printf("The old ID has been changed successfully\n");
			 return ;// id is changed successfully
		}
		else
		{
			searchID = searchID->nextTask;
		}
	}

	printf("the entered oldID doesn't exist\n");
	return ;//id is not found
}				
ADD_CMD("changetaskID",changeTaskID,"changeTaskID newID oldID")

/* FUNCTION      : buildFunctions
 * DESCRIPTION   : This function add a new function
				   to the functions pool so the user can
				   use the name to add tasks pool by using 
				   "TaskAdd" function
 * PARAMETERS    : void(*f)(void *data), char *funcName
 * RETURNS       : int type
*/	
int buildFunctions(void(*f)(void *data), char *funcName)
{
	uint8_t i =0;
	for(i = 0 ; i<EXSITED_FUCNTION_SIZE;i++)
	{
		if(strcmp(availablefucntions[i].Name, funcName)==0) return 0;// function name is already in the pool
		if(availablefucntions[i].f==NULL)
		{
			availablefucntions[i].f = f;
			availablefucntions[i].Name = funcName;
			return 1;
		}
	}  	
	return -1; // function pool is full
}

/* FUNCTION      : checkID
 * DESCRIPTION   :This function checks for new Task ID
				  against the existed tasks IDs
 * PARAMETERS    : void(*f)(void *data), char *funcName
 * RETURNS       : int type
*/	

int checkID(Task_t* current, Task_t* new)
{
		if(new->taskID == current->taskID)
		{
			printf("The task ID is already exist\n");
			free(new);
			return -1;
		}
	return 0;
}
