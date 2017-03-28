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
#include "lwt.h"
#include "debug.h"
/* End Config Includes *******************************************************/

/* Public Global Variables ***************************************************/
extern void ipc_test(void* param);
s32 cur_tid=1;
s32 glb_thd_num=0;
s32 count_lwt_info[INFO_SIZE];
/* End Public Global Variables ***********************************************/


/* Begin Function:__lwt_schedule *********************************************
Description : Switch to another thread.
Input       : lwt_t thd - The thread who calls schedule.
Output      : glb_head.next - The next thread to run.
Return      : None.
******************************************************************************/
void
__lwt_schedule(lwt_t thd)
{
    struct list_head* info;

    /* put the current guy at the end of the queue */
    info=glb_head.next;
    sys_delete_node(&glb_head,glb_head.next->next);
    sys_insert_node(info,glb_head.prev,&glb_head);


    if(thd!=NULL)
    {
    /* put this guy at the beginning of the queue */
    sys_delete_node(thd->head.prev,thd->head.next);
    sys_insert_node(&thd->head,&glb_head,glb_head.next);
    }

    /* Don't do any scheduling if we are scheduling to ourselves */
    if(info==glb_head.next)
    return;

    __lwt_dispatch(&((list_to_lwt_t(info))->context),&((list_to_lwt_t(glb_head.next))->context));
}
/* End Function:__lwt_schedule ***********************************************/

/* Begin Function:__lwt_trampoline ********************************************
Description : The execution point where execution for a new thread begins.
Input       : None.
Output      : None.
Return      : None.
******************************************************************************/
void
__lwt_trampoline(void)
{

  lwt_current()->entry(lwt_current()->data);

  (list_to_lwt_t(glb_head.next))->retval=lwt_current()->data;

  lwt_die(0);

}
/* End Function: __lwt_trampoline*********************************************/

/* Begin Function:__lwt_stack_get *********************************************
Description : Allocates a new stack for a new lwt.
Input       : None.
Output      : A new stack.
Return      : Pointer which points to the address of new stack.
******************************************************************************/
void*
__lwt_stack_get(void)
{
    /* Always 5kB stack */
    return malloc(STACK_SIZE);
}
/* End Function: __lwt_stack_get*********************************************/

/* Begin Function:lwt_create*******************************************
Description : Create a thread, this function calls the passed in fn
              with the argument passed in as data.
Input       : lwt_fn_t fn, void* data.
Output      : lwt_t info -Thread we create.
Return      : lwt_t info -Thread we create.
*******************************************************************************/
lwt_t
lwt_create(lwt_fn_t fn, void* data)
{
    lwt_t info;

    if(fn==NULL) return NULL;
    info=lwt_create_by_condition(info,fn);

    /* put this thread into the last position of the queue */
    sys_insert_node(&info->head,glb_head.prev,&glb_head);

    /* do some stack init (safe redundancy 1000) */
    info->context.sp=(((u32)(info->init_stack))+STACK_SIZE-SAFE_REDUNDANCY);
    *((void**)(info->context.sp)+1)=data;
    info->context.ip=__lwt_trampoline;
    info->data=data;

    /* Increase global reference */
    glb_thd_num++;
    count_lwt_info[LWT_INFO_NTHD_RUNNABLE]++;
    info->tid=glb_thd_num;

    if(glb_thd_num==1)
    	info->ptid=0;
    else
    	info->ptid=list_to_lwt_t((glb_head.next))->tid;

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
void
__lwt_destroy(lwt_t thd)
{
    /* remove this guy from whatever queue it is in now */
    sys_delete_node(thd->head.prev,thd->head.next);
    lwt_destroy_by_condition(thd);
}
/* End Function: __lwt_destroy*************************************************/


/* Begin Function:lwt_join*****************************************************
Description : It blocks waiting for the referenced lwt to terminate.
Input       : lwt_t thd - The thread it waiting for.
Output      : None.
Return      : dead_retval - returns the void * that the thread itself
returned from its lwt fn t, or that it passed to lwt die.
******************************************************************************/
void*
lwt_join(lwt_t thd)
{
    lwt_t curr;
    void* dead_retval;

    /* go through the zombie list to see if that guy have already dead. */
    struct list_head* trav_ptr;
    trav_ptr=zombie_head.next;
    curr=list_to_lwt_t(glb_head.next);

    while(trav_ptr!=&zombie_head)
    {
        if((list_to_lwt_t(trav_ptr))->tid==thd->tid)
        break;

        trav_ptr=trav_ptr->next;
	}

    if(trav_ptr==&zombie_head)
    {
    	if(thd->ptid!=curr->tid)
    		return -1;

        /* that guy is not dead. insert me into the waiting list and never execute me until... */
        sys_delete_node(curr->head.prev,curr->head.next);
        sys_insert_node(&curr->head,wait_head.prev,&wait_head);

        curr->wait_id=thd->tid;

        count_lwt_info[LWT_INFO_NTHD_BLOCKED]++;
        count_lwt_info[LWT_INFO_NTHD_RUNNABLE]--;

        __lwt_dispatch(&(curr->context),&((list_to_lwt_t(glb_head.next))->context));
        /* I'm invoked by somebody. must be that guy. */
	}

    dead_retval=thd->data;

    /* destroy this zombie, we already have its data */
    if(thd->ptid==curr->tid)
    {
		__lwt_destroy(thd);
		count_lwt_info[LWT_INFO_NTHD_ZOMBIES]--;
    }

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
void
lwt_die(void* data)
{
    /* go through the wait list to see if there is anyone waiting for this.
     *  if yes, return immediately, and resume the execution of that guy.
     */

    struct list_head* trav_ptr;
    lwt_t thd;

    thd=list_to_lwt_t(glb_head.next);
    /* Insert me into the zombie queue */
    sys_delete_node(thd->head.prev,thd->head.next);
    sys_insert_node(&thd->head,zombie_head.prev,&zombie_head);

    count_lwt_info[LWT_INFO_NTHD_RUNNABLE]--;
    count_lwt_info[LWT_INFO_NTHD_ZOMBIES]++;

    /* my return value */
    thd->retval=data;
    trav_ptr=wait_head.next;

    while(trav_ptr!=&wait_head)
    {
        if((list_to_lwt_t(trav_ptr))->wait_id==thd->tid)
        {
            (list_to_lwt_t(trav_ptr))->retval=data;

            thd=list_to_lwt_t(trav_ptr);

            /* put this guy to the last of the thread queue, we are beginning to schedule it */
            sys_delete_node(thd->head.prev,thd->head.next);
            sys_insert_node(&thd->head,glb_head.prev,&glb_head);

            count_lwt_info[LWT_INFO_NTHD_BLOCKED]--;
            count_lwt_info[LWT_INFO_NTHD_RUNNABLE]++;

            break;
        }
        trav_ptr=trav_ptr->next;
    }

    /* yield now, never execute me anymore */
    __lwt_dispatch(&((list_to_lwt_t(zombie_head.prev))->context),&((list_to_lwt_t(glb_head.next))->context));

    /* should never reach here */
    printf("error in die\n");
    while(1);
}
/* End Function: lwt_die*************************************************/

/* Begin Function:lwt_yield********************************************************
Description : Yield the currently executing lwt, and possibly switch to another lwt
Input       : lwt_t thd - Attempts to switch directly to the specified lwt.
Output      : None.
Return      : (list_to_lwt_t(glb_head.next))->tid - Id of next thread.
***********************************************************************************/
s32
lwt_yield(lwt_t thd)
{
    __lwt_schedule(thd);

    return (list_to_lwt_t(glb_head.next))->tid;
}
/* End Function: lwt_yield********************************************************/

/* Begin Function:lwt_current******************************************************
Description : Return the currently active thread.
Input       : None.
Output      : None.
Return      : list_to_lwt_t(glb_head.next).
***********************************************************************************/
lwt_t
lwt_current(void)
{
	lwt_t cur=list_to_lwt_t(glb_head.next);
	cur->state=LWT_RUNNING;
    return cur;
}
/* End Function: lwt_current*******************************************************/

/* Begin Function:lwt_id************************************************************
Description : Return the unique identifier for the thread.
Input       : None.
Output      : None.
Return      : thd->tid.
***********************************************************************************/
s32
lwt_id(lwt_t thd)
{
    return thd->tid;
}
/* End Function: lwt_id*******************************************************/


/* Begin Function:lwt_info*****************************************************
Description :  A debugging helper.
               lwt info t is an enum including
               LWT INFO NTHD RUNNABLE, LWT INFO NTHD BLOCKED, LWT INFO NTHD ZOMBIES,
               etc.Depending on which of these is passed in, lwt info returns the
               number of threads that are either runnable,blocked or that have died.
Input       : None.
Output      : None.
Return      : count_lwt_info[t].
******************************************************************************/
s32
lwt_info(lwt_info_t t)
{
    return count_lwt_info[t];
}
/* End Function: lwt_info*****************************************************/

/* Begin Function:__lwt_start ********************************************
Description : Initialize
              context.sp - Stack pointer.
              context.ip - Instruction pointer.
Input       : None.
Output      : None.
Return      : None.
******************************************************************************/
void
__lwt_start(void)
{
    struct lwt_context context;
    context.sp=(u32)&context;
    context.ip=(u32)__lwt_trampoline;
    /* not used - empty function */
    __lwt_dispatch(&context,&((list_to_lwt_t(glb_head.next))->context));

}
/* End Function: __lwt_start*********************************************/

/* Begin Function:lwt_chan ****************************************************
Description : Allocate a lwt channel. The channel number always increases.
Input       : None.
Output      : None.
Return      : lwt_chan_t - The pointer to the channel created.
******************************************************************************/
lwt_chan_t lwt_chan(s32 sz)
{
	lwt_chan_t chan;
	chan=malloc(sizeof(struct lwt_channel));
	sys_create_list(&(chan->head));
	chan->chgp=NULL;
	chan->buf_size=sz;
	chan->snd_cnt=0;
	chan->blocked_num=0;
	chan->mark=NULL;
	chan->rcv_data=list_to_lwt_t(glb_head.next);
	sys_create_list(&chan->sending);
	sys_create_list(&chan->receiving);
	count_lwt_info[LWT_INFO_NCHAN]++;
	return chan;
}
/* End Function:lwt_chan *****************************************************/

/* Begin Function:lwt_chan_deref **********************************************
Description : Allocate a lwt channel. The channel number always increases.
Input       : None.
Output      : None.
Return      : count_lwt_info[t].
******************************************************************************/
void lwt_chan_deref(lwt_chan_t c)
{
	if((c->snd_cnt==0)&&(c->rcv_data==0)){
		free(c);
		count_lwt_info[LWT_INFO_NCHAN]--;
	}
	else
	{
		if(list_to_lwt_t(glb_head.next)==c->rcv_data)
			c->rcv_data=0;
		else
			c->snd_cnt--;
	}
}
/* End Function:lwt_chan_deref ***********************************************/

/* Begin Function:lwt_snd *****************************************************
Description : Send something to one channel.
Input       : lwt_chan_t c - The channel to send to.
              void* data - The data to send.
Output      : None.
Return      : s32 - 0 for success, -1 for failure.
******************************************************************************/
s32 lwt_snd(lwt_chan_t c, void* data)
{
	lwt_t sndrcv_thd;
	lwt_t grprcv_thd;
	/* See if there are any receivers in the channel */
	if(c->rcv_data==0)
		return -1;

	/* Anyone currently receiving? */
	if((c->receiving.next)!=&(c->receiving))
	{
		/* Yes. invoke that guy and don't block */
		sndrcv_thd=list_to_lwt_t(c->receiving.next);
		sys_delete_node(sndrcv_thd->head.prev,sndrcv_thd->head.next);
		sys_insert_node(&(sndrcv_thd->head),glb_head.prev,&glb_head);
		sndrcv_thd->sndrcv_data=data;
		sndrcv_thd->sndrcv_len=c->buf_size;
		count_lwt_info[LWT_INFO_NRCVING]--;
		count_lwt_info[LWT_INFO_NTHD_RUNNABLE]++;
	}
	else
	{
		sndrcv_thd=list_to_lwt_t(glb_head.next);
		/* Nobody is receiving. Now we just block */
		sys_delete_node(sndrcv_thd->head.prev,sndrcv_thd->head.next);
		sys_insert_node(&(sndrcv_thd->head),c->sending.prev,&(c->sending));
		sndrcv_thd->sndrcv_data=data;
		sndrcv_thd->sndrcv_len=c->buf_size;
		count_lwt_info[LWT_INFO_NTHD_RUNNABLE]--;
		count_lwt_info[LWT_INFO_NSNDING]++;

		/* See if this guy is in any channel group. If yes, we just put this into the channel group's
		 * pending list.
		 */
		if((c->chgp!=0)&&(c->blocked_num==0))
		{
			sys_delete_node(c->head.prev,c->head.next);
			sys_insert_node(&(c->head),c->chgp->pend_head.prev,&(c->chgp->pend_head));
			c->chgp->pend_cnt++;
			/* Check if there is a thread pending on this channel group. If there is, wake it up */
			if(c->chgp->pend_head.next!=&(c->chgp->pend_head))
			{
				grprcv_thd=list_to_lwt_t(c->chgp->pend_head.next);
				sys_delete_node(grprcv_thd->head.prev,grprcv_thd->head.next);
				sys_insert_node(&(grprcv_thd->head),glb_head.prev,&glb_head);
				count_lwt_info[LWT_INFO_NRCVGRPING]--;
				count_lwt_info[LWT_INFO_NTHD_RUNNABLE]++;
			}
		}
		c->blocked_num++;

		__lwt_dispatch(&(sndrcv_thd->context),&((list_to_lwt_t(glb_head.next))->context));
	}

	return 0;
}
/* End Function:lwt_snd ******************************************************/

/* Begin Function:lwt_rcv *****************************************************
Description : Receive something from one channel.
Input       : lwt_chan_t c - The channel to receive from.
Output      : None.
Return      : sndrcv_thd->sndrcv_data.
******************************************************************************/
void*
lwt_rcv(lwt_chan_t c)
{
	lwt_t sndrcv_thd;
	/* See if there are any senders in the channel */
	if(c->snd_cnt==0)
		return -1;

	/* Anyone currently sending? */
	if((c->sending.next)!=&(c->sending))
	{
		/* Yes. invoke that guy and don't block */
		sndrcv_thd=list_to_lwt_t(c->sending.next);
		sys_delete_node(sndrcv_thd->head.prev,sndrcv_thd->head.next);
		sys_insert_node(&(sndrcv_thd->head),glb_head.prev,&glb_head);
		count_lwt_info[LWT_INFO_NSNDING]--;
		count_lwt_info[LWT_INFO_NTHD_RUNNABLE]++;
		/* See if the channel have a channel group. If yes, we need to reevaluate the
		 * status of this channel in the channel group.
		 */
		if((c->chgp!=0)&&(c->blocked_num==1))
		{
			sys_delete_node(c->head.prev,c->head.next);
			sys_insert_node(&(c->head),c->chgp->idle_head.prev,&(c->chgp->idle_head));
			c->chgp->pend_cnt--;
		}
		c->blocked_num--;

		return (void*)sndrcv_thd->sndrcv_data;
	}
	else
	{
		sndrcv_thd=list_to_lwt_t(glb_head.next);
		/* Nobody is receiving. Now we just block */
		sys_delete_node(sndrcv_thd->head.prev,sndrcv_thd->head.next);
		sys_insert_node(&(sndrcv_thd->head),c->receiving.prev,&(c->receiving));
		count_lwt_info[LWT_INFO_NRCVING]++;
		count_lwt_info[LWT_INFO_NTHD_RUNNABLE]--;
		c->snd_cnt++;

		__lwt_dispatch(&(sndrcv_thd->context),&((list_to_lwt_t(glb_head.next))->context));
	}

	return sndrcv_thd->sndrcv_data;
}
/* End Function:lwt_rcv ******************************************************/

/* Begin Function:lwt_create_chan *********************************************
Description : Create a thread and send the channel as a parameter.
Input       : lwt_chan_t c - The channel to send to.
              void* data - The data to send.
Output      : None.
Return      : s32 - 0 for success, -1 for failure.
******************************************************************************/
lwt_t
lwt_create_chan(lwt_fn_t fn, lwt_chan_t c)
{
	c->snd_cnt++;
	return lwt_create(fn,c);
}
/* End Function:lwt_create_chan **********************************************/

/* Begin Function:lwt_snd_chan *****************************************************
Description : Send channel to one channel (sending is sent over c).
Input       : lwt_chan_t c - The channel to send to.
              lwt_chan_t sending- The channel to send.
Output      : None.
Return      : s32 - 0 for success, -1 for failure.
******************************************************************************/
s32
lwt_snd_chan(lwt_chan_t c,lwt_chan_t sending)
{
	/* See if there are any receivers in the channel */
		if(c->rcv_data==0)
			return -1;

	lwt_snd(c,sending);

	sending->snd_cnt++;

	return 0;

}
/* End Function:lwt_snd_chan **********************************************/

/* Begin Function:lwt_rcv_chan *********************************************
Description : Receive a channel.
Input       : lwt_chan_t c - The channel to receive from.
Output      : None.
Return      : lwt_chan_t - received channel.
***************************************************************************/
lwt_chan_t
lwt_rcv_chan(lwt_chan_t c)
{
	return lwt_rcv(c);
}
/* End Function:lwt_rcv_chan **********************************************/

/* Begin Function:lwt_cgrp *************************************************
Description : Create a channel group.
Input       : None.
Output      : None.
Return      : lwt_cgrp_t - The channel group returned.
***************************************************************************/
lwt_cgrp_t
lwt_cgrp(void)
{
	/* Allocate the channel group */
	lwt_cgrp_t cgrp;
	cgrp=malloc(sizeof(struct lwt_channel_group));
	sys_create_list(&(cgrp->idle_head));
	sys_create_list(&(cgrp->pend_head));
	cgrp->chan_cnt=0;
	cgrp->pend_cnt=0;
	count_lwt_info[LWT_INFO_NCGRP]++;
	return cgrp;
}
/* End Function:lwt_cgrp **************************************************/

/* Begin Function:lwt_cgrp_free ********************************************
Description : Create a channel group.
Input       : lwt_chan_t c - The channel to receive from.
Output      : None.
Return      : int - If successful,0; else return -1.
***************************************************************************/
int
lwt_cgrp_free(lwt_cgrp_t cgrp)
{
	lwt_chan_t temp;
	/* Free the channel group */
	if(cgrp->pend_cnt!=0)
		return -1;

	/* Remove all the channels from the channel group */
	while((cgrp->idle_head.next)!=&(cgrp->idle_head))
	{
		temp=list_to_chan_t(cgrp->idle_head.next);
		temp->chgp=0;
		sys_delete_node(temp->head.prev,temp->head.next);
	}

	count_lwt_info[LWT_INFO_NCGRP]--;
	free(cgrp);
	return 0;
}
/* End Function:lwt_cgrp_free *********************************************/

/* Begin Function:lwt_cgrp_add *********************************************
Description : Add a channel to a channel froup.
Input       : lwt_cgrp t cgrp - The channel group to add to.
  	  	  	  lwt_chan_t chan - The channel to add to the channel group.
Output      : None.
Return      : lwt_chan_t - received channel.
***************************************************************************/
int
lwt_cgrp_add(lwt_cgrp_t cgrp, lwt_chan_t chan)
{
	/* The channel have been added to other channel groups? */
	if(chan->chgp!=0)
		return -1;

	/* Does the chrrent channel group have any channel attached to it? */
	if(cgrp->chan_cnt==0)
	{
		cgrp->rcv_data=chan->rcv_data;
	}
	else
	{
		/* Are the channels in one group compatible? */
		if(cgrp->rcv_data!=chan->rcv_data)
			return -1;
	}
	sys_insert_node(&(chan->head),cgrp->idle_head.prev,&(cgrp->idle_head));
	cgrp->chan_cnt++;
	return 0;
}
/* End Function:lwt_cgrp_add **********************************************/

/* Begin Function:lwt_cgrp_rem *********************************************
Description : Remove a channel from the channel group.
Input       : lwt_cgrp_t cgrp - The channel group.
              lwt_chan_t chan - The channel to remove.
Output      : None.
Return      : int - If there is a pending event on the channel, return 1;
                    if the channel does not belong to the channel group, return -1;
                    if successful, 0.
***************************************************************************/
int
lwt_cgrp_rem(lwt_cgrp_t cgrp, lwt_chan_t chan)
{
	/* Does this channel belong to the channel group? */
	if(chan->chgp!=cgrp)
		return -1;

	/* Is there are a event on this channel? - must be a channel */
	if(chan->sending.next!=&(chan->sending))
		return 1;

	/* We can safely remove it now */
	sys_delete_node(chan->head.prev,chan->head.next);
	cgrp->chan_cnt--;
	chan->chgp=0;
	return 0;
}
/* End Function:lwt_cgrp_rem **********************************************/

/* Begin Function:lwt_cgrp_wait ********************************************
Description : Wait for a channel group.
Input       : lwt_cgrp_t cgrp - The channel group to receive from.
Output      : None.
Return      : lwt_chan_t - received channel.
***************************************************************************/
lwt_chan_t
lwt_cgrp_wait(lwt_cgrp_t cgrp)
{
	lwt_t rcvgrp_thd;
	/* see if there are channels blocked. If there is, receive from it */
	if(cgrp->pend_cnt!=0)
		return cgrp->pend_head.next;

	/* Nobody sending. Block me */
	count_lwt_info[LWT_INFO_NRCVGRPING]++;
	count_lwt_info[LWT_INFO_NTHD_RUNNABLE]--;
	rcvgrp_thd=list_to_lwt_t(glb_head.next);
	sys_delete_node(rcvgrp_thd->head.prev,rcvgrp_thd->head.next);
	sys_insert_node(&(rcvgrp_thd->head),cgrp->recv_head.prev,&(cgrp->recv_head));
	__lwt_dispatch(&(rcvgrp_thd->context),&((list_to_lwt_t(glb_head.next))->context));

	/* I'm unblocked. There must be some channel pending. Get that channel and return. */
	return cgrp->pend_head.next;
}
/* End Function:lwt_cgrp_wait *********************************************/

/* Begin Function:lwt_chan_mark_set ****************************************
Description : Associate a channel with a mark.
Input       : lwt_chan_t c - The channel to associate with.
              void* mark - The mark to associate with the channel.
Output      : None.
Return      : lwt_chan_t - received channel.
***************************************************************************/
void
lwt_chan_mark_set(lwt_chan_t c, void* mark)
{
	c->mark=mark;
}
/* End Function:lwt_chan_mark_set *****************************************/

/* Begin Function:lwt_chan_mark_get ****************************************
Description : Get the mark associated with the channel.
Input       : lwt_chan_t c - The channel to consult mark.
Output      : None.
Return      : void* - The mark
***************************************************************************/
void*
lwt_chan_mark_get(lwt_chan_t c)
{
	return c->mark;
}
/* End Function:lwt_chan_mark_get *****************************************/

s32 test2(void* data)
{
	s32* myvar=malloc(4);
	*myvar=1234;
	lwt_snd(data,myvar);
	return 0;
}

s32 test(void* data)
{
	lwt_chan_t channel=lwt_chan(4);
	lwt_t sender=lwt_create_chan(test2,channel);
	s32* myvar=lwt_rcv(channel);
	printf("I received %d\n",*myvar);
	free(myvar);
	lwt_join(sender);
	exit(0);
}



s32
main(void)
{
    lwt_t thread1;

    sys_create_list(&glb_head);
    sys_create_list(&zombie_head);
    sys_create_list(&wait_head);
    sys_create_list(&pool_head);

    thread1=lwt_create(ipc_test,1234);

    /* Never return */
    __lwt_start();

    puts("!!!Hello World!!!"); /* prints !!!Hello World!!! */
    return EXIT_SUCCESS;
}
