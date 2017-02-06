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

#include <stdio.h>
#include <stdlib.h>
#include "lwt.h"
extern void test(void* param);


struct list_head glb_head;
struct list_head wait_head;
struct list_head zombie_head;
struct list_head pool_head;
int cur_tid=1;
int glb_thd_num=0;
int count_thd[SIZE_OF_LWT_INFO];



void LWT_INLINE
__lwt_schedule(lwt_t thd)
{
	struct list_head* info;

	/* put the current guy at the end of the queue */
	info=glb_head.next;
	glb_head.next=info->next;
	glb_head.next->prev=&glb_head;

	info->prev=glb_head.prev;
	info->next=&glb_head;
	glb_head.prev->next=info;
	glb_head.prev=info;

	if(thd!=0)
	{
		/* put this guy at the beginning of the queue */
		thd->head.prev->next=thd->head.next;
		thd->head.next->prev=thd->head.prev;

		thd->head.prev=&glb_head;
		thd->head.next=glb_head.next;
		glb_head.next->prev=&(thd->head);
		glb_head.next=&(thd->head);
	}

	/* Don't do any scheduling if we are scheduling to ourselves */
	if(info==glb_head.next)
		return;

	__lwt_dispatch(&(((lwt_t)info)->context),&(((lwt_t)(glb_head.next))->context));
}

void LWT_INLINE
__lwt_trampoline(void)
{
	struct lwt_context context;
	context.sp=(unsigned long)&context;
	context.ip=(unsigned long)__lwt_trampoline;
	/* not used - empty function */
	__lwt_dispatch(&context,&(((lwt_t)(glb_head.next))->context));
}

void* LWT_INLINE
__lwt_stack_get(void)
{
	/* Always 5kB stack */
	return malloc(5000);
}

void LWT_INLINE
__lwt_stack_return(void)
{
	/* not used - empty function */
}

lwt_t LWT_INLINE
lwt_create(lwt_fn_t fn, void* data)
{
	lwt_t info;
	/* TODO:check if fn and data are valid pointers */

#ifdef LWT_USE_POOL

	if(pool_head.next!=&pool_head){

		info=pool_head.next;

		pool_head.next=pool_head.next->next;
		pool_head.next->prev=&pool_head;

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
	info->head.prev=glb_head.prev;
	info->head.next=&glb_head;
	glb_head.prev->next=&(info->head);
	glb_head.prev=&(info->head);

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

void LWT_INLINE
__lwt_destroy(lwt_t thd)
{
	/* remove this guy from whatever queue it is in now */
	thd->head.prev->next=thd->head.next;
	thd->head.next->prev=thd->head.prev;


#ifdef LWT_USE_POOL
	thd->head.prev=pool_head.prev;
	thd->head.next=&pool_head;
	pool_head.prev->next=&(thd->head);
	pool_head.prev=&(thd->head);

#else
	free(thd->init_stack);
	free(thd);
#endif
}

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
		curr->head.prev->next=curr->head.next;
		curr->head.next->prev=curr->head.prev;

		curr->head.prev=wait_head.prev;
		curr->head.next=&wait_head;
		wait_head.prev->next=&(curr->head);
		wait_head.prev=&(curr->head);

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
	thd->head.prev->next=thd->head.next;
	thd->head.next->prev=thd->head.prev;

	thd->head.prev=zombie_head.prev;
	thd->head.next=&zombie_head;
	zombie_head.prev->next=&(thd->head);
	zombie_head.prev=&(thd->head);

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
			thd->head.prev->next=thd->head.next;
			thd->head.next->prev=thd->head.prev;

			thd->head.prev=glb_head.prev;
			thd->head.next=&glb_head;
			glb_head.prev->next=&(thd->head);
			glb_head.prev=&(thd->head);

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

int LWT_INLINE
lwt_yield(lwt_t thd)
{
	__lwt_schedule(thd);
	return ((lwt_t)glb_head.next)->tid;
}


lwt_t LWT_INLINE
lwt_current(void)
{
	return (lwt_t)glb_head.next;
}

int LWT_INLINE
lwt_id(lwt_t thd)
{
	return thd->tid;
}

int LWT_INLINE
lwt_info(lwt_info_t t)
{
	return count_thd[t];
}

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

	glb_head.prev=&glb_head;
	glb_head.next=&glb_head;

	wait_head.prev=&wait_head;
	wait_head.next=&wait_head;

	zombie_head.prev=&zombie_head;
	zombie_head.next=&zombie_head;

	pool_head.prev=&pool_head;
	pool_head.next=&pool_head;

	thread1=lwt_create(test,1234);


	/* Never return */
	__lwt_trampoline();



	puts("!!!Hello World!!!"); /* prints !!!Hello World!!! */
	return EXIT_SUCCESS;
}
