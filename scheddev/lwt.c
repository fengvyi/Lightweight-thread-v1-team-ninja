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

void* local_sto[4096];

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

/* Memory facilities */
static Header base;
static Header *freep;

/* The following addition is for M:N scheduling with the sl */
static inline void set_stk_data(unsigned long stack_ptr,int offset, long data)
{
	/*
	 * We save the CPU_ID and thread id in the stack for fast
	 * access.  We want to find the struct cos_stk (see the stkmgr
	 * interface) so that we can then offset into it and get the
	 * cpu_id.  This struct is at the _top_ of the current stack,
	 * and cpu_id is at the top of the struct (it is a u32_t).
	 */
    printc("\n\nThe ***set*** stk data stack frame:%x\n\n",(stack_ptr & ~(COS_STACK_SZ - 1)));
	*(long *)((stack_ptr & ~(COS_STACK_SZ - 1))+COS_STACK_SZ - offset * sizeof(u32_t))=data;
}

void cos_set_thd_id(unsigned long stack_ptr,long thdid)
{
    set_stk_data(stack_ptr,THDID_OFFSET,thdid);
}


/*
void
free(void *ap)
{
  Header *bp, *p;

  bp = (Header*)ap - 1;
  for(p = freep; !(bp > p && bp < p->s.ptr); p = p->s.ptr)
    if(p >= p->s.ptr && (bp > p || bp < p->s.ptr))
      break;
  if(bp + bp->s.size == p->s.ptr){
    bp->s.size += p->s.ptr->s.size;
    bp->s.ptr = p->s.ptr->s.ptr;
  } else
    bp->s.ptr = p->s.ptr;
  if(p + p->s.size == bp){
    p->s.size += bp->s.size;
    p->s.ptr = bp->s.ptr;
  } else
    p->s.ptr = bp;
  freep = p;
}

static char* lwt_sbrk(uint size)
{
    int count;
    char* p = cos_page_bump_alloc(cos_defcompinfo_curr_get());

    if(size<=4096)
        return p;

    for(count=0;count<size-4096;count+=4096)
    {
        cos_page_bump_alloc(cos_defcompinfo_curr_get());
    }

    return p;
}

static Header*
morecore(uint nu)
{
  char *p;
  Header *hp;

  if(nu < 4096)
    nu = 4096;
  p = lwt_sbrk(nu * sizeof(Header));
  if(p == (char*)-1)
    return 0;
  hp = (Header*)p;
  hp->s.size = nu;
  free((void*)(hp + 1));
  return freep;
}

void* 
malloc(uint nbytes)
{
  Header *p, *prevp;
  uint nunits;

  nunits = (nbytes + sizeof(Header) - 1)/sizeof(Header) + 1;
  if((prevp = freep) == 0){
    base.s.ptr = freep = prevp = &base;
    base.s.size = 0;
  }
  for(p = prevp->s.ptr; ; prevp = p, p = p->s.ptr){
    if(p->s.size >= nunits){
      if(p->s.size == nunits)
        prevp->s.ptr = p->s.ptr;
      else {
        p->s.size -= nunits;
        p += p->s.size;
        p->s.size = nunits;
      }
      freep = prevp;
      return (void*)(p + 1);
    }
    if(p == freep)
      if((p = morecore(nunits)) == 0)
        return 0;
  }
}
*/

void* malloc(unsigned int size)
{
    int count;
    printc("memalloc:");
    char* p = cos_page_bump_alloc(cos_defcompinfo_curr_get());
    printc(":%x\n",p);

    if(size<=4096)
        return p;

    for(count=0;count<size-4096;count+=4096)
    {
        cos_page_bump_alloc(cos_defcompinfo_curr_get());
    }

    return p;
}

void free(void* size)
{
    
}
/* Begin Function:sys_enqueue *************************************************
Description : Enqueue a number into the queue. We do not need to detect if the
              queue is full because it never will. The number of the threads in
              the system never exceeds 4096, thus even if you add all of them
              to the queue, the queue will not overflow.
Input       : ccq_t queue - The queue to operate.
              void* data - The data to enqueue.
Output      : None.
Return      : None.
******************************************************************************/
void sys_enqueue(ccq_t queue, void* data)
{
    unsigned long ptr;
    //printc("begin enqueue\n");
    /* We do not check if we can enque. surely we can, because we know how many
     * threads there gonna be in the system. even when they are all in one queue,
     * the queue will not be full. */
    ptr=CCQ_ROUND(lwt_faa(&(queue->head_ptr),1));
    
    /* It is possible that the guy haven't completed his data read yet. wait for it to complete */
    while(queue->buf[ptr]!=0)
        kthd_yield();

    queue->buf[ptr]=data;
    //printc("end enqueue\n");
    return 0;
}
/* End Function:sys_enqueue **************************************************/

/* Begin Function:sys_dequeue *************************************************
Description : This dequeue function assumes a single consumer. For multiple consumers
              it is not guaranteed to be correct. This dequeue function will always
              be successful. When dequeuing is not possible, we just block.
Input       : ccq_t queue - The queue to operate.
Output      : None.
Return      : void* - The data we just dequeued.
******************************************************************************/
void* sys_dequeue(ccq_t queue)
{
    unsigned long ptr;
    void* retval;

    /* See if we are capable of dequeueing anything - in fact no faa needed here */
    ptr=CCQ_ROUND(lwt_faa(&(queue->tail_ptr),1));

    while(queue->buf[ptr]==0)
        kthd_yield();

    retval=queue->buf[ptr];
    queue->buf[ptr]=0;

    /* Do some sort of cas */
    return retval;
}
/* End Function:sys_dequeue **************************************************/

/* Begin Function:sys_dqtest **************************************************
Description : Test if the queue is dequeuable. If yes, return 0; else -1.
Input       : ccq_t queue - The queue to operate.
Output      : None.
Return      : 0 for possible, -1 for not.
******************************************************************************/
int sys_dqtest(ccq_t queue)
{
	if(queue->buf[CCQ_ROUND(queue->tail_ptr)]==0)
		return -1;
	else
		return 0;
}
/* End Function:sys_dqtest ***************************************************/

/* Queue facilities */
void LWT_INLINE kthd_yield(void)
{
   sl_thd_yield(0);
}

/*
void LWT_INLINE kthd_block(void)
{
    
}

void LWT_INLINE kthd_unblock(kthd_t kthd)
{
    
}
*/
void LWT_INLINE lwt_kthd_do_ready(void)
{
    lwt_t thd;
    kthd_t kthd=(kthd_t)tls_get(0);
	while(sys_dqtest(&(kthd->ready_queue))!=-1)
    {
        //printc("Dequeue ready_queue\n");
        thd=sys_dequeue(&(kthd->ready_queue));
        //printc("Thd dequed:  %x\n",thd); 
        sys_insert_node(&thd->head,(kthd->glb_head).prev,&(kthd->glb_head));
        //printc("Thd inserted\n");
    }

    if((kthd->glb_head).next==&(kthd->glb_head))
    {
        while(sys_dqtest(&(kthd->ready_queue))==-1)
            kthd_yield();

	    while(sys_dqtest(&(kthd->ready_queue))!=-1)
        {
            thd=sys_dequeue(&(kthd->ready_queue));
            sys_insert_node(&thd->head,(kthd->glb_head).prev,&(kthd->glb_head));
        }
    }
}



static void LWT_INLINE
__lwt_dispatch(struct lwt_context *curr, struct lwt_context *next)
{
    lwt_kthd_do_ready();
    kthd_t kthd=(kthd_t)tls_get(0);
    next=&((list_to_lwt_t((kthd->glb_head).next))->context);
    //printc("jumping to %x:%x\n",next->ip,next->sp);

	__asm__ __volatile__(
	"pusha \n\t"
	"movl %%esp, %0 \n\t"
	"movl $1f, %1 \n\t"
	"movl %2, %%esp \n\t"
	"jmp *%3 \n\t"
	"1: popa \n\t"
	:"=m"(curr->sp), "=m"(curr->ip)
	:"m"(next->sp), "m"(next->ip)
	:
	);
}

void sys_queue_init(ccq_t queue)
{
    memset(queue,0,sizeof(struct cc_queue));
}

/* Begin Function:__lwt_schedule *********************************************
Description : Switch to another thread.
Input       : lwt_t thd - The thread who calls schedule.
Output      : (kthd->glb_head).next - The next thread to run.
Return      : None.
******************************************************************************/
void
__lwt_schedule(lwt_t thd)
{
    struct list_head* info;
    /* The current kthd is in the TLS */
    kthd_t kthd=(kthd_t)tls_get(0);

    /* put the current guy at the end of the queue */
    info=(kthd->glb_head).next;
    sys_delete_node(&(kthd->glb_head),(kthd->glb_head).next->next);
    sys_insert_node(info,(kthd->glb_head).prev,&(kthd->glb_head));


    if(thd!=NULL)
    {
    /* put this guy at the beginning of the queue */
    sys_delete_node(thd->head.prev,thd->head.next);
    sys_insert_node(&thd->head,&(kthd->glb_head),(kthd->glb_head).next);
    }

    /* Don't do any scheduling if we are scheduling to ourselves */
    if(info==(kthd->glb_head).next)
    return;

    __lwt_dispatch(&((list_to_lwt_t(info))->context),&((list_to_lwt_t((kthd->glb_head).next))->context));
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
  /* The current kthd is in the TLS */
  kthd_t kthd=(kthd_t)tls_get(0);
  printc("trampo kthd: %x", kthd);
  //printc("on the tramp\n");
  lwt_current()->entry(lwt_current()->data);
  printc("entry kthd: %x", kthd);
  (list_to_lwt_t((kthd->glb_head).next))->retval=lwt_current()->data;
  lwt_die(0);

}
/* End Function: __lwt_trampoline*********************************************/


void* stkalloc(kthd_t kthd)
{
    if(kthd->stack_alloc[0]==0)
    {
        kthd->stack_alloc[0]=1;
        return kthd->stack_addr;
    }
    else if(kthd->stack_alloc[1]==0)
    {
        kthd->stack_alloc[1]=1;
        return kthd->stack_addr+STACK_SIZE;
    }
    else if(kthd->stack_alloc[2]==0)
    {
        kthd->stack_alloc[2]=1;
        return kthd->stack_addr+STACK_SIZE*2;
    }
    else if(kthd->stack_alloc[3]==0)
    {
        kthd->stack_alloc[3]=1;
        return kthd->stack_addr+STACK_SIZE*3;
    } 
    else
        return 0;
}

void stkfree(void* stack, kthd_t kthd)
{
    
}


/* Begin Function:__lwt_stack_get *********************************************
Description : Allocates a new stack for a new lwt.
Input       : None.
Output      : A new stack.
Return      : Pointer which points to the address of new stack.
******************************************************************************/
void*
__lwt_stack_get(kthd_t kthd)
{
    /* Always 4096 stack */
    return malloc(4096);
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
    /* The current kthd is in the TLS */
    kthd_t kthd=(kthd_t)tls_get(0);
    if(fn==NULL) return NULL;
    info=lwt_create_by_condition(info,fn);

    /* put this thread into the last position of the queue */
    sys_insert_node(&info->head,(kthd->glb_head).prev,&(kthd->glb_head));

    /* do some stack init (safe redundancy 1000) */
    info->context.sp=(((u32)(info->init_stack))+STACK_SIZE-SAFE_REDUNDANCY);
    *((void**)(info->context.sp)+1)=data;
    info->context.ip=__lwt_trampoline;
    info->data=data;

    /* Increase global reference */
    lwt_faa(&glb_thd_num,1);
    lwt_faa(&count_lwt_info[LWT_INFO_NTHD_RUNNABLE],1);
    info->tid=glb_thd_num;

    if(glb_thd_num==1)
    	info->ptid=0;
    else
    	info->ptid=list_to_lwt_t(((kthd->glb_head).next))->tid;

    info->wait_id=0;

    return info;
}

lwt_t
__lwt_create(lwt_fn_t fn, void* data, kthd_t kthd)
{
    lwt_t info;
    if(fn==NULL) return NULL;

    info=__lwt_create_by_condition(info,fn,kthd);
    printc("create by condition successful!\n");
    printc("kthd:%d\n",info->kthd);
    /* put this thread into the last position of the queue */
    sys_insert_node(&info->head,(kthd->glb_head).prev,&(kthd->glb_head));
    
    /* do some stack init (safe redundancy 1000) */
    info->context.sp=(((u32)(info->init_stack))+STACK_SIZE-SAFE_REDUNDANCY);
    *((void**)(info->context.sp)+1)=data;
    info->context.ip=__lwt_trampoline;
    printc("trampoline addr: %x\n",__lwt_trampoline);
    info->data=data;

    /* Increase global reference */
    lwt_faa(&glb_thd_num,1);
    lwt_faa(&count_lwt_info[LWT_INFO_NTHD_RUNNABLE],1);
    info->tid=glb_thd_num;

    if(glb_thd_num==1)
    	info->ptid=0;
    else
    	info->ptid=list_to_lwt_t(((kthd->glb_head).next))->tid;

    info->wait_id=0;

    return info;
}
/* End Function: lwt_create****************************************************/

/* Begin Function:__lwt_destroy*************************************************
Description : Destroy a thread, free the memory or put it into (kthd->pool_head) so that
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
    /* The current kthd is in the TLS */
    kthd_t kthd=(kthd_t)tls_get(0);

    /* go through the zombie list to see if that guy have already dead. */
    struct list_head* trav_ptr;
    trav_ptr=(kthd->zombie_head).next;
    curr=list_to_lwt_t((kthd->glb_head).next);

    while(trav_ptr!=&(kthd->zombie_head))
    {
        if((list_to_lwt_t(trav_ptr))->tid==thd->tid)
        break;

        trav_ptr=trav_ptr->next;
	}

    if(trav_ptr==&(kthd->zombie_head))
    {
    	if(thd->ptid!=curr->tid)
    		return -1;

        /* that guy is not dead. insert me into the waiting list and never execute me until... */
        sys_delete_node(curr->head.prev,curr->head.next);
        sys_insert_node(&curr->head,(kthd->wait_head).prev,&(kthd->wait_head));

        curr->wait_id=thd->tid;

        lwt_faa(&count_lwt_info[LWT_INFO_NTHD_BLOCKED],1);
        lwt_faa(&count_lwt_info[LWT_INFO_NTHD_RUNNABLE],-1);

        __lwt_dispatch(&(curr->context),&((list_to_lwt_t((kthd->glb_head).next))->context));
        /* I'm invoked by somebody. must be that guy. */
	}

    dead_retval=thd->data;

    /* destroy this zombie, we already have its data */
    if(thd->ptid==curr->tid)
    {
		__lwt_destroy(thd);
		lwt_faa(&count_lwt_info[LWT_INFO_NTHD_ZOMBIES],-1);
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
    /* The current kthd is in the TLS */
    kthd_t kthd=(kthd_t)tls_get(0);
    /* go through the wait list to see if there is anyone waiting for this.
     *  if yes, return immediately, and resume the execution of that guy.
     */

    struct list_head* trav_ptr;
    lwt_t thd;

    thd=list_to_lwt_t((kthd->glb_head).next);
    /* Insert me into the zombie queue */
    sys_delete_node(thd->head.prev,thd->head.next);
    sys_insert_node(&thd->head,(kthd->zombie_head).prev,&(kthd->zombie_head));

    lwt_faa(&count_lwt_info[LWT_INFO_NTHD_RUNNABLE],-1);
    lwt_faa(&count_lwt_info[LWT_INFO_NTHD_ZOMBIES],1);

    /* my return value */
    thd->retval=data;
    trav_ptr=(kthd->wait_head).next;

    while(trav_ptr!=&(kthd->wait_head))
    {
        if((list_to_lwt_t(trav_ptr))->wait_id==thd->tid)
        {
            (list_to_lwt_t(trav_ptr))->retval=data;

            thd=list_to_lwt_t(trav_ptr);

            /* put this guy to the last of the thread queue, we are beginning to schedule it */
            sys_delete_node(thd->head.prev,thd->head.next);
            sys_insert_node(&thd->head,(kthd->glb_head).prev,&(kthd->glb_head));

            lwt_faa(&count_lwt_info[LWT_INFO_NTHD_BLOCKED],-1);
            lwt_faa(&count_lwt_info[LWT_INFO_NTHD_RUNNABLE],1);

            break;
        }
        trav_ptr=trav_ptr->next;
    }
    /* yield now, never execute me anymore */
    __lwt_dispatch(&((list_to_lwt_t((kthd->zombie_head).prev))->context),&((list_to_lwt_t((kthd->glb_head).next))->context));

    /* should never reach here */
    printc("error in die\n");
    while(1);
}
/* End Function: lwt_die*************************************************/

/* Begin Function:lwt_yield********************************************************
Description : Yield the currently executing lwt, and possibly switch to another lwt
Input       : lwt_t thd - Attempts to switch directly to the specified lwt.
Output      : None.
Return      : (list_to_lwt_t((kthd->glb_head).next))->tid - Id of next thread.
***********************************************************************************/
s32
lwt_yield(lwt_t thd)
{
    /* The current kthd is in the TLS */
    kthd_t kthd=(kthd_t)tls_get(0);

    __lwt_schedule(thd);

    return (list_to_lwt_t((kthd->glb_head).next))->tid;
}
/* End Function: lwt_yield********************************************************/

/* Begin Function:lwt_current******************************************************
Description : Return the currently active thread.
Input       : None.
Output      : None.
Return      : list_to_lwt_t((kthd->glb_head).next).
***********************************************************************************/
lwt_t
lwt_current(void)
{
    /* The current kthd is in the TLS */
    kthd_t kthd=(kthd_t)tls_get(0);
    printc("current kthd:%x\n", kthd);
    //printc("thd kthd in current func : %x:%d", kthd,cos_thdid());
	lwt_t cur=list_to_lwt_t((kthd->glb_head).next);
	cur->state=LWT_RUNNING;
    //printc("The current lwt thread is %x\n",cur);
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

    /* The current kthd is in the TLS */
    kthd_t kthd=(kthd_t)tls_get(0);
    printc("tls in start:  %x\n",kthd);

    context.sp=(u32)&context;
    context.ip=(u32)__lwt_trampoline;
    /* not used - empty function */
    __lwt_dispatch(&context,&((list_to_lwt_t((kthd->glb_head).next))->context));

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
    printc("lwt_chan begin\n"); 
	lwt_chan_t chan;
    /* The current kthd is in the TLS */
    kthd_t kthd=(kthd_t)tls_get(0);
    printc("kthd:%d\n",kthd);
    printc("About to malloc a chan\n"); 
	chan=malloc(sizeof(struct lwt_channel));
    printc("chan malloc complete\n");
	sys_create_list(&(chan->head));
    printc("chan head created\n");
	chan->chgp=NULL;
	chan->buf_size=sz;
	chan->snd_cnt=0;
	chan->blocked_num=0;
	chan->mark=NULL;
    printc("About to set rcv_data\n");
	chan->rcv_data=list_to_lwt_t((kthd->glb_head).next);
    printc("chan init complete\n");
    /* Modify this to the queue implementation */
    sys_queue_init(&chan->sending);   //sys_create_list(&chan->sending);
    chan->receiving=0;                //sys_create_list(&chan->receiving);

	lwt_faa(&count_lwt_info[LWT_INFO_NCHAN],1);
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
    /* The current kthd is in the TLS */
    kthd_t kthd=(kthd_t)tls_get(0);

	if((c->snd_cnt==0)&&(c->rcv_data==0)){
		free(c);
		lwt_faa(&count_lwt_info[LWT_INFO_NCHAN],-1);
	}
	else
	{
		if(list_to_lwt_t((kthd->glb_head).next)==c->rcv_data)
			c->rcv_data=0;
		else
			lwt_faa(&c->snd_cnt,-1);
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
    //printc("start snd\n");
    /* The current kthd is in the TLS */
    kthd_t kthd=(kthd_t)tls_get(0);

	/* See if there are any receivers in the channel */
	if(c->rcv_data==0)
		return -1;

	/* Anyone currently receiving? */
	if(c->receiving!=0)//((c->receiving.next)!=&(c->receiving))
	{
        //printc("going to invoke the receive guy\n");
		/* Yes. invoke that guy and don't block */
		sndrcv_thd=c->rcv_data;//sndrcv_thd=list_to_lwt_t(c->receiving.next);
		c->receiving=0;//sys_delete_node(sndrcv_thd->head.prev,sndrcv_thd->head.next);
        //printc("Thd invoked:  %x\n",sndrcv_thd);        

		sndrcv_thd->sndrcv_data=data;
		sndrcv_thd->sndrcv_len=c->buf_size;

		sys_enqueue(&(sndrcv_thd->kthd->ready_queue),sndrcv_thd);//sys_insert_node(&(sndrcv_thd->head),(kthd->glb_head).prev,&(kthd->glb_head));
        
        //printc("Receive guy invoked\n");

		lwt_faa(&count_lwt_info[LWT_INFO_NRCVING],-1);
		lwt_faa(&count_lwt_info[LWT_INFO_NTHD_RUNNABLE],1);
	}
	else
	{
		sndrcv_thd=list_to_lwt_t((kthd->glb_head).next);
		/* Nobody is receiving. Now we just block */
        //printc("Nobody is receiving. Now we just block\n");
		sys_delete_node(sndrcv_thd->head.prev,sndrcv_thd->head.next);
        
		sndrcv_thd->sndrcv_data=data;
		sndrcv_thd->sndrcv_len=c->buf_size;

		sys_enqueue(&(c->sending),sndrcv_thd);//sys_insert_node(&(sndrcv_thd->head),c->sending.prev,&(c->sending));
        //printc("Thd blocked:  %x\n",sndrcv_thd);
		lwt_faa(&count_lwt_info[LWT_INFO_NTHD_RUNNABLE],-1);
		lwt_faa(&count_lwt_info[LWT_INFO_NSNDING],1);

		/* See if this guy is in any channel group. If yes, we just put this into the channel group's
		 * pending list.
		 */
		if(c->chgp!=0)//((c->chgp!=0)&&(c->blocked_num==0))
		{
            if(lwt_faa(&(c->blocked_num),1)==1)
            {
                sys_enqueue(&(c->chgp->pend_queue),c);
			    //sys_delete_node(c->head.prev,c->head.next);
			    //sys_insert_node(&(c->head),c->chgp->pend_head.prev,&(c->chgp->pend_head));
			    //c->chgp->pend_cnt++;
			    /* Check if there is a thread pending on this channel group. If there is, wake it up */
			    if(c->chgp->receiving!=0)//(c->chgp->pend_head.next!=&(c->chgp->pend_head))
			    {
				    grprcv_thd=c->chgp->rcv_data;//list_to_lwt_t(c->chgp->pend_head.next);
				    //sys_delete_node(grprcv_thd->head.prev,grprcv_thd->head.next);
				    sys_enqueue(&(grprcv_thd->kthd->ready_queue),grprcv_thd);//sys_insert_node(&(grprcv_thd->head),(kthd->glb_head).prev,&(kthd->glb_head));
				    lwt_faa(&count_lwt_info[LWT_INFO_NRCVGRPING],-1);
				    lwt_faa(&count_lwt_info[LWT_INFO_NTHD_RUNNABLE],1);
			    }
            }
		}
        else
		    lwt_faa(&c->blocked_num,1);

		__lwt_dispatch(&(sndrcv_thd->context),&((list_to_lwt_t((kthd->glb_head).next))->context));
	}

	return 0;
}
/* End Function:lwt_snd ******************************************************/

void lwt_cgrp_do_pend(lwt_cgrp_t chgp)
{
    printc("do pend begin\n");
    lwt_chan_t c;
    while(sys_dqtest(&(chgp->pend_queue))!=-1)
    {
        c=sys_dequeue(&(chgp->pend_queue));
        sys_delete_node(c->head.prev,c->head.next);
	    sys_insert_node(&(c->head),c->chgp->pend_head.prev,&(c->chgp->pend_head));
	    lwt_faa(&c->chgp->pend_cnt,1);
    }
}


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

    /* The current kthd is in the TLS */
    kthd_t kthd=(kthd_t)tls_get(0);

    /* Deal with all pending operations */
    if(c->chgp!=0)
        lwt_cgrp_do_pend(c->chgp);

	/* See if there are any senders in the channel */
	if(c->snd_cnt==0)
		return -1;

	/* Anyone currently sending? */
	if(sys_dqtest(&(c->sending))!=(-1))//(c->sending.next)!=&(c->sending))
	{
        //printc("going to invoke the send guy\n");
		/* Yes. invoke that guy and don't block */
		sndrcv_thd=sys_dequeue(&(c->sending));   //list_to_lwt_t(c->sending.next);
        //printc("Thd invoked:  %x\n",sndrcv_thd); 
		//sys_delete_node(sndrcv_thd->head.prev,sndrcv_thd->head.next);
        // printc("sndrcv_thd->kthd->ready_queue:  %x\n",sndrcv_thd->kthd);
		sys_enqueue(&(sndrcv_thd->kthd->ready_queue),sndrcv_thd);//sys_insert_node(&(sndrcv_thd->head),(kthd->glb_head).prev,&(kthd->glb_head));

        //printc("thd enqued\n");
		lwt_faa(&count_lwt_info[LWT_INFO_NSNDING],-1);
		lwt_faa(&count_lwt_info[LWT_INFO_NTHD_RUNNABLE],1);
		/* See if the channel have a channel group. If yes, we need to reevaluate the
		 * status of this channel in the channel group.
		 */
		if((c->chgp!=0)&&(c->blocked_num==1))
		{
            //sys_enqueue(&(c->chgp->idle_queue),c);
			sys_delete_node(c->head.prev,c->head.next);
			sys_insert_node(&(c->head),c->chgp->idle_head.prev,&(c->chgp->idle_head));
			//lwt_faa(&c->chgp->pend_cnt,-1);
		}
        //printc("if-finished\n");
		lwt_faa(&c->blocked_num,-1);
        //printc("return data=%d",(int)sndrcv_thd->sndrcv_data);
		return (void*)sndrcv_thd->sndrcv_data;
	}
	else
	{
		sndrcv_thd=list_to_lwt_t((kthd->glb_head).next);
		/* Nobody is sending. Now we just block */
        //printc("Nobody is sending. Now we just block\n");
		sys_delete_node(sndrcv_thd->head.prev,sndrcv_thd->head.next);
		c->receiving=1;//sys_insert_node(&(sndrcv_thd->head),c->receiving.prev,&(c->receiving));
        c->rcv_data=sndrcv_thd;
        //printc("Thd blocked:  %x\n",sndrcv_thd);
		lwt_faa(&count_lwt_info[LWT_INFO_NRCVING],1);
		lwt_faa(&count_lwt_info[LWT_INFO_NTHD_RUNNABLE],-1);
		lwt_faa(&c->snd_cnt,1);

		__lwt_dispatch(&(sndrcv_thd->context),&((list_to_lwt_t((kthd->glb_head).next))->context));
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
    if(c!=0)
	    lwt_faa(&c->snd_cnt,1);
	return lwt_create(fn,c);
}

lwt_t
__lwt_create_chan(lwt_fn_t fn, lwt_chan_t c, kthd_t kthd)
{
    if(c!=0)
	    lwt_faa(&c->snd_cnt,1);
    printc("ready to do lwt creation\n");
	return __lwt_create(fn, c, kthd);
}
/* End Function:lwt_create_chan **********************************************/

/* Begin Function:lwt_snd_chan ************************************************
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

	lwt_faa(&sending->snd_cnt,1);

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
    sys_queue_init(&(cgrp->pend_queue));
	cgrp->chan_cnt=0;
	cgrp->pend_cnt=0;
	lwt_faa(&count_lwt_info[LWT_INFO_NCGRP],1);
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
    /* The current kthd is in the TLS */
    kthd_t kthd=(kthd_t)tls_get(0);

    /* Deal with all pending operations */
    lwt_cgrp_do_pend(cgrp);

	/* Free the channel group */
	if(cgrp->pend_cnt!=0){
       printc("cgrp->pend_cnt: %d\n",cgrp->pend_cnt);
		return -1;
}

	/* Remove all the channels from the channel group */
	while((cgrp->idle_head.next)!=&(cgrp->idle_head))
	{
		temp=list_to_chan_t(cgrp->idle_head.next);
		temp->chgp=0;
		sys_delete_node(temp->head.prev,temp->head.next);
	}

	lwt_faa(&count_lwt_info[LWT_INFO_NCGRP],-1);
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
    chan->chgp=cgrp;
	lwt_faa(&cgrp->chan_cnt,1);
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
	if(sys_dqtest(&(chan->sending))!=-1)//chan->sending.next!=&(chan->sending))
		return 1;

	/* We can safely remove it now */
	sys_delete_node(chan->head.prev,chan->head.next);
	lwt_faa(&cgrp->chan_cnt,-1);
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
    /* The current kthd is in the TLS */
    kthd_t kthd=(kthd_t)tls_get(0);

    /* Deal with all pending operations */
    lwt_cgrp_do_pend(cgrp);

	/* see if there are channels blocked. If there is, receive from it */
	if(cgrp->pend_cnt!=0)
		return cgrp->pend_head.next;

	/* Nobody sending. Block me */
	lwt_faa(&count_lwt_info[LWT_INFO_NRCVGRPING],1);
	lwt_faa(&count_lwt_info[LWT_INFO_NTHD_RUNNABLE],-1);

	rcvgrp_thd=list_to_lwt_t((kthd->glb_head).next);
	sys_delete_node(rcvgrp_thd->head.prev,rcvgrp_thd->head.next);
	cgrp->receiving=1;//sys_insert_node(&(rcvgrp_thd->head),cgrp->recv_head.prev,&(cgrp->recv_head));

	__lwt_dispatch(&(rcvgrp_thd->context),&((list_to_lwt_t((kthd->glb_head).next))->context));
    /* Deal with pending operations here again */
    lwt_cgrp_do_pend(cgrp);

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

/* Begin Function:__lwt_sl_fn **********************************************
Description : The code that the sl kernel thread will run.
Input       : void* data - The data to pass into this thread.
Output      : None.
Return      : None.
***************************************************************************/
void __lwt_sl_fn(void *data)
{
    local_sto[cos_thdid()]=data;
    /* This is our own data structure */
    printc("The original tls content:  %x",data);
    printc("The kernel thread id:  %d\n",cos_thdid());
    //tls_set(0,data);
    printc("The tls content:  %x",tls_get(0));
    /* Run the threads one by one until they yield themselves, we never return */
    __lwt_start();
}
/* End Function:__lwt_sl_fn ************************************************/

void* stack_frame_get(void)
{
    void* curr_sp;

	__asm__ __volatile__(
	"movl %%esp, %0 \n\t"
	:"=m"(curr_sp)
	:
	:
	);

    printc("current stack %x, stack frame %x\n",curr_sp,((unsigned long)curr_sp)&0xFFFFF000);
    return (void*)(((unsigned long)curr_sp)&0xFFFFF000);
}

/* Begin Function:lwt_kthd_create *******************************************
Description : Create a thread that is associated with a kthd.
Input       : lwt_fn_t fn - The function that will run as a thread.
              lwt_chan_t c - The channel that is passed to the lwt-thread created.
Output      : None.
Return      : kthd_t - The kernel thread we created.
***************************************************************************/
kthd_t lwt_kthd_create(lwt_fn_t fn, lwt_chan_t c)
{
    lwt_t thd;
    kthd_t kthd;
	union sched_param sp= {.c = {.type=SCHEDP_PRIO, .value=10}};

    /* Allocate a sl structure */
    kthd=malloc(sizeof(struct lwt_sl_thread));
    kthd->kthd_struct=0;

    /* Initialize the lists for this guy */
    sys_create_list(&(kthd->glb_head));
    sys_create_list(&(kthd->wait_head));
    sys_create_list(&(kthd->zombie_head));
    sys_create_list(&(kthd->pool_head));
    sys_queue_init(&(kthd->ready_queue));
    printc("list creation successful\n");
    kthd->kthd_struct=sl_thd_alloc(__lwt_sl_fn,kthd);
/*
    kthd->stack_alloc[0]=0;
    kthd->stack_alloc[1]=0;
    kthd->stack_alloc[2]=0;
    kthd->stack_alloc[3]=0;
    kthd->stack_addr=stack_frame_get(); */
    printc("kthd allocation successful!\n");
    sl_thd_param_set(kthd->kthd_struct, sp.v);
    printc("parameter setting successful!\n");
    /* Tie the thread's local memory to its kernel TLS */
    //cos_thd_mod(cos_compinfo_get(cos_defcompinfo_curr_get()), kthd->kthd_struct->thdcap, &kthd);
    /* Create a lwt thread and a channel */
    thd=__lwt_create_chan(fn,c,kthd);
    
    printc("lwt thd creation complete\n");
    return kthd;
}
/* End Function:lwt_kthd_create ********************************************/

/* Begin Function:lwt_snd_thd ***********************************************
Description : Send a thread over the channel.
Input       : lwt_chan_t c - The channel sent.
              lwt_t sending - The thread that we are sending.
Output      : None.
Return      : None.
***************************************************************************/
int lwt_snd_thd(lwt_chan_t c, lwt_t sending) 
{
    /* add a parameter "belong" to the thread struct. The thread that is sent must be removed from the runqueue, or any queue it is in, first */
    return lwt_snd(c, sending);
}
/* End Function:lwt_snd_thd ***********************************************/

/* Begin Function:lwt_rcv_thd ***********************************************
Description : Receive a thread over the channel.
Input       : lwt_chan_t c - The channel sent.
              lwt_t sending - The thread that we are sending.
Output      : None.
Return      : None.
***************************************************************************/
lwt_t lwt_rcv_thd(lwt_chan_t c) 
{
    return lwt_rcv(c);
}
/* End Function:lwt_snd_thd ***********************************************/

/* The testing function is moved to the unit_schedlib.c */

