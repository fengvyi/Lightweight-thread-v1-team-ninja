/*
 ============================================================================
 Name        : lwt.c
 Author      : ZF Song, RY Pan
 Version     :
 Copyright   : Ninja
 Description : Hello World in C, Ansi-style
               Dispatch is now done. exit and join.
               exit:(while(1) and save the result to somewhere)
               join: when called,
                     1(check if the func is dead. If already dead, clena up)
                     2if not died wait for death.
                     3 get something from the struct, unless that func exit is called we put this into the run queue.
 ============================================================================
 */

/* Config Includes ***********************************************************/
#include <stdio.h>
#include <stdlib.h>
#include "lwt.h"
/* End Config Includes *******************************************************/

/* Public Global Variables ***************************************************/
extern void test(void* param);
int cur_tid=1;
int glb_thd_num=0;
int count_thd[SIZE_OF_LWT_INFO];
/* End Public Global Variables ***********************************************/

/* Begin Function:__lwt_schedule *********************************************
Description : Switch to another thread.
Input       : lwt_t thd - The thread who calls schedule.
Output      : glb_head.next - The next thread to run.
Return      : None.
******************************************************************************/
void LWT_INLINE
__lwt_schedule(lwt_t thd)
{
	struct list_head* info;

	/* put the current guy at the end of the queue */
	info=glb_head.next;
	Sys_delete_node(&glb_head,glb_head.next->next);

	Sys_insert_node(info,&glb_head,glb_head.next);

	if(thd!=0)
	{
		/* put this guy at the beginning of the queue */

		Sys_delete_node(thd->head.prev,thd->head.next);

		Sys_insert_node(&thd->head,&glb_head,glb_head.next);
	}

	/* Don't do any scheduling if we are scheduling to ourselves */
	if(info==glb_head.next)
		return;

	__lwt_dispatch(&(((lwt_t)info)->context),&(((lwt_t)(glb_head.next))->context));
}
/* End Function:__lwt_schedule ***********************************************/

/* Begin Function:__lwt_trampoline ********************************************
Description : Initialize
			  context.sp - Stack pointer.
			  context.ip - Instruction pointer.
Input       : None.
Output      : None.
Return      : None.
******************************************************************************/
void LWT_INLINE
__lwt_trampoline(void)
{
	struct lwt_context context;
	context.sp=(unsigned long)&context;
	context.ip=(unsigned long)__lwt_trampoline;
	/* not used - empty function */
	__lwt_dispatch(&context,&(((lwt_t)(glb_head.next))->context));
}
/* End Function: __lwt_trampoline*********************************************/

/* Begin Function:__lwt_stack_get *********************************************
Description : Allocates a new stack for a new lwt.
Input       : None.
Output      : A new stack.
Return      : Pointer which points to the address of new stack.
******************************************************************************/
void* LWT_INLINE
__lwt_stack_get(void)
{
	/* Always 5kB stack */
	return malloc(5000);
}
/* End Function: __lwt_stack_get*********************************************/

/* Begin Function:__lwt_stack_return******************************************
Description : Not used - empty function.
Input       : None.
Output      : None.
Return      : None.
******************************************************************************/
void LWT_INLINE
__lwt_stack_return(void)
{
	/* not used - empty function */
}
/* End Function: __lwt_stack_return*******************************************/

/* Begin Function:__lwt_stack_return*******************************************
Description : Create a thread, this function calls the passed in fn
              with the argument passed in as data.
Input       : lwt_fn_t fn, void* data.
Output      : lwt_t info -Thread we create.
Return      : lwt_t info -Thread we create.
*******************************************************************************/
lwt_t LWT_INLINE
lwt_create(lwt_fn_t fn, void* data)
{
	lwt_t info;
	/* TODO:check if fn and data are valid pointers */

#ifdef LWT_USE_POOL

	if(pool_head.next!=&pool_head){

		info=pool_head.next;

		Sys_delete_node(&pool_head,pool_head.next->next);

		info->entry=fn;

	}else{


	info=malloc(64/*sizeof(struct lwt_info_struct)*/);
	/* TODO:check if info allocation is successful */
	info->entry=fn;
	/* TODO:check if stack allocation is successful */
	info->init_stack=__lwt_stack_get();

	}
#else

	info=malloc(sizeof(struct lwt_info_struct));
		/* TODO:check if info allocation is successful */
		info->entry=fn;
		/* TODO:check if stack allocation is successful */
		info->init_stack=__lwt_stack_get();

#endif

	/* put this thread into the last position of the queue */

	Sys_insert_node(&info->head,glb_head.prev,&glb_head);

	/* do some stack init (safe redundancy 1000) */
	info->context.sp=(((unsigned long)(info->init_stack))+5000-1000);
	*((void**)(info->context.sp)+1)=data;
	info->context.ip=(unsigned long)(info->entry);
	/* Increase global reference */
	glb_thd_num++;
	count_thd[LWT_INFO_NTHD_RUNNABLE]++;
	info->tid=glb_thd_num;
	info->wait_id=0;

	return info;
}
/* End Function: lwt_create****************************************************/

/* Begin Function:__lwt_destroy*************************************************
Description : Destroy a thread, free the memory or put it into pool_head so that
              that thread can be reused quickly for new lwt create calls.
Input       : lwt_t thd - thread we want to destroy.
Output      : None.
Return      : None.
*******************************************************************************/
void LWT_INLINE
__lwt_destroy(lwt_t thd)
{
	/* remove this guy from whatever queue it is in now */
	Sys_delete_node(thd->head.prev,thd->head.next);


#ifdef LWT_USE_POOL

	Sys_insert_node(&(thd->head),pool_head.prev,&pool_head);

#else
	free(thd->init_stack);
	free(thd);
#endif
}
/* End Function: __lwt_destroy*************************************************/


/* Begin Function:lwt_join*****************************************************
Description : It blocks waiting for the referenced lwt to terminate.
Input       : lwt_t thd - The thread it waiting for.
Output      : None.
Return      : dead_retval - returns the void * that the thread itself
returned from its lwt fn t, or that it passed to lwt die.
******************************************************************************/
void* LWT_INLINE
lwt_join(lwt_t thd)
{
	lwt_t curr;
	void* dead_retval;

	/* go through the zombie list to see if that guy have already dead. */
	struct list_head* trav_ptr;

	trav_ptr=zombie_head.next;
	while(trav_ptr!=&zombie_head)
	{
		if(((lwt_t)trav_ptr)->tid==thd->tid)
			break;

		trav_ptr=trav_ptr->next;
	}

	if(trav_ptr==&zombie_head)
	{
		curr=(lwt_t)glb_head.next;
		/* that guy is not dead. insert me into the waiting list and never execute me until... */
		Sys_delete_node(curr->head.prev,curr->head.next);

		Sys_insert_node(&curr->head,wait_head.prev,&wait_head);

		curr->wait_id=thd->tid;

		count_thd[LWT_INFO_NTHD_BLOCKED]++;
		count_thd[LWT_INFO_NTHD_RUNNABLE]--;

		__lwt_dispatch(&(curr->context),&(((lwt_t)(glb_head.next))->context));
		/* I'm invoked by somebody. must be that guy. */
	}

	dead_retval=thd->retval;

	/* destroy this zombie, we already have its data */
	__lwt_destroy((lwt_t)thd);

	count_thd[LWT_INFO_NTHD_ZOMBIES]--;

	/* return the value */
	return dead_retval;
}
/* End Function: lwt_join*****************************************************/

/* Begin Function:lwt_die******************************************************
Description : Kill the current thread.
Input       : void* data.
Output      : None.
Return      : None.
******************************************************************************/
void LWT_INLINE
lwt_die(void* data)
{
	/* go through the wait list to see if there is anyone waiting for this.
	 *  if yes, return immediately, and resume the execution of that guy.
	 */

	struct list_head* trav_ptr;
	lwt_t thd;

	thd=glb_head.next;
	/* Insert me into the zombie queue */

	Sys_delete_node(thd->head.prev,thd->head.next);

	Sys_insert_node(&thd->head,zombie_head.prev,&zombie_head);

	count_thd[LWT_INFO_NTHD_RUNNABLE]--;
	count_thd[LWT_INFO_NTHD_ZOMBIES]++;

	/* my return value */
	thd->retval=data;

	trav_ptr=wait_head.next;
	while(trav_ptr!=&wait_head)
	{
		if(((lwt_t)trav_ptr)->wait_id==thd->tid)
		{
			((lwt_t)trav_ptr)->retval=data;

			thd=(lwt_t)trav_ptr;

			/* put this guy to the last of the thread queue, we are beginning to schedule it */

			Sys_delete_node(thd->head.prev,thd->head.next);

			Sys_insert_node(&thd->head,glb_head.prev,&glb_head);

			count_thd[LWT_INFO_NTHD_BLOCKED]--;
			count_thd[LWT_INFO_NTHD_RUNNABLE]++;

			break;
		}
		trav_ptr=trav_ptr->next;
	}

	/* yield now, never execute me anymore */
	__lwt_dispatch(&(((lwt_t)(zombie_head.prev))->context),&(((lwt_t)(glb_head.next))->context));

	/* should never reach here */
	printf("error in die\n");
	while(1);
}
/* End Function: lwt_die*************************************************/

/* Begin Function:lwt_yield********************************************************
Description : Yield the currently executing lwt, and possibly switch to another lwt
Input       : lwt_t thd - Attempts to switch directly to the specified lwt.
Output      : None.
Return      : ((lwt_t)glb_head.next)->tid - Id of next thread.
***********************************************************************************/
int LWT_INLINE
lwt_yield(lwt_t thd)
{
	__lwt_schedule(thd);
	return ((lwt_t)glb_head.next)->tid;
}
/* End Function: lwt_yield********************************************************/

/* Begin Function:lwt_current******************************************************
Description : Return the currently active thread.
Input       : None.
Output      : None.
Return      : (lwt_t)glb_head.next.
***********************************************************************************/
lwt_t LWT_INLINE
lwt_current(void)
{
	return (lwt_t)glb_head.next;
}
/* End Function: lwt_current*******************************************************/

/* Begin Function:lwt_id************************************************************
Description : Return the unique identifier for the thread.
Input       : None.
Output      : None.
Return      : thd->tid.
***********************************************************************************/
int LWT_INLINE
lwt_id(lwt_t thd)
{
	return thd->tid;
}
/* End Function: lwt_id***********************************************************/

/* Begin Function:lwt_info********************************************************
Description :  A debugging helper.
               lwt info t is an enum including
               LWT INFO NTHD RUNNABLE, LWT INFO NTHD BLOCKED, LWT INFO NTHD ZOMBIES.
               Depending on which of these is passed in, lwt info returns the number
               of threads that are either runnable,blocked or that have died.
Input       : None.
Output      : None.
Return      : count_thd[t].
**********************************************************************************/
int LWT_INLINE
lwt_info(lwt_info_t t)
{
	return count_thd[t];
}
/* End Function: lwt_info*********************************************************/

/*
void func1(void* data)
{
	int a,b,c;

	while(1)
	{
		c=b+a;
		b=(unsigned int)data+c;
	//	lwt_yield(0);
		printf("%s\n","Thread2:");
		printf("%s\n","Zombie List before join:");
		printf("%d\n",lwt_info(LWT_INFO_NTHD_ZOMBIES));

		lwt_join((lwt_t)data);

		printf("%s\n","Zombie List after join:");
		printf("%d",lwt_info(LWT_INFO_NTHD_ZOMBIES));

		exit(0);
	}
}

void func2(void* data)
{
	int a,b,c;

	while(1)
	{
		printf("%s\n","Thread1 call die()");
		c=b*a;
		b=(unsigned int)data+c;
		lwt_die(1234);
		lwt_yield(0);

	}
}*/

int
main(void)
{
	lwt_t thread1;

    Sys_init_list();

	thread1=lwt_create(test,1234);


	/* Never return */
	__lwt_trampoline();



	puts("!!!Hello World!!!"); /* prints !!!Hello World!!! */
	return EXIT_SUCCESS;
}
