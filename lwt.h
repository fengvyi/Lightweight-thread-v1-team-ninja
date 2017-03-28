/*
 ============================================================================
 Name        : lwt.h
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

/* Defines *******************************************************************/
#ifndef LWT_H_
#define LWT_H_
#define LWT_NULL 0
#define LWT_INLINE __attribute__((always_inline))
#define STACK_SIZE 5000
#define SAFE_REDUNDANCY 1000
#define offsetof(type, member) (unsigned long)&(((type*)0)->member)
#define container_of(ptr, type, member) ({ \
        const typeof( ((type *)0)->member ) *__mptr = (ptr); \
        (type *)( (char *)__mptr - offsetof(type,member) );})

#define list_to_lwt_t(x) container_of(x,struct lwt_info_struct,head)
#define list_to_chan_t(x) container_of(x,struct lwt_channel,head)
/* End Defines ***************************************************************/


/* Config Includes ***********************************************************/
#include <stdio.h>
#include <stdlib.h>
#include "sys_list.h"
/* End Config Includes *******************************************************/

/* Typedef *******************************************************************/
typedef signed int s32;
typedef signed short s16;
typedef signed char  s8;

typedef unsigned long long u64;
typedef unsigned int  u32;
typedef unsigned short u16;
typedef unsigned char  u8;

typedef void(*lwt_fn_t)(void* data);

typedef enum
{
    LWT_INFO_NTHD_RUNNABLE = 0,
    LWT_INFO_NTHD_BLOCKED  = 1,
    LWT_INFO_NTHD_ZOMBIES  = 2,
	LWT_INFO_NCHAN = 3,
	LWT_INFO_NSNDING = 4,
	LWT_INFO_NRCVING = 5,
	LWT_RUNNING = 6,
	LWT_INFO_NCGRP=7,
	LWT_INFO_NRCVGRPING=8,
    INFO_SIZE
} lwt_info_t;
/* End Typedef ***************************************************************/


/* Begin Struct: lwt_info_struct **********************************************
Description : The informations of a thread.
******************************************************************************/
struct lwt_context {
	unsigned long ip, sp;
};

struct lwt_info_struct
{
    struct list_head head;
    s32 tid;
    s32 ptid;
    s32 wait_id;
    lwt_fn_t entry;
    void* init_stack;
    struct lwt_context context;
    void* retval;
    void* data;
    void* sndrcv_data;
    s32 sndrcv_len;
    s32 state;
};

struct lwt_channel_group
{
	/* The list head for non-pending events */
	struct list_head idle_head;
	struct list_head pend_head;
	/* If the receiver is blocked, it will be here */
	struct list_head recv_head;
	s32 chan_cnt;
	s32 pend_cnt;
	/* The receiver of the channel group - must be the same as the channels */
	struct lwt_info_struct* rcv_data;
};

struct lwt_channel
{
	/* This head is for lwt_list */
	struct list_head head;
	/* This is for the lwt channel grouping */
	struct lwt_channel_group* chgp;
	/* Blocked counter */
	s32 blocked_num;
	/* Associated mark */
	void* mark;
	s32 buf_size;
	/* Reference counting - the number of senders and receivers */
	s32 snd_cnt;
	/* The receiver of the channel */
	struct lwt_info_struct* rcv_data;
	/* The threads that are currently receiving, but nobody have called send */
	struct list_head receiving;
	/* The threads that are currently sending, but nobody have called receive */
	struct list_head sending;
};



static void LWT_INLINE
__lwt_dispatch(struct lwt_context *curr, struct lwt_context *next){
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


/*
static void LWT_INLINE
__lwt_dispatch(struct lwt_context *curr, struct lwt_context *next){
	__asm__ __volatile__(
	"pusha \n\t"
	"movl %%esp, %0 \n\t"
	"movl $1f, %1 \n\t"
	"movl %2, %%esp \n\t"
	"movl %3, %%ebx \n\t"
	"jmp *%%ebx \n\t"
	"1: popa \n\t"
	:"=m"(curr->sp), "=m"(curr->ip)
	:"r"(next->sp), "r"(next->ip)
	:"eax","ebx"
	);
}*/
/* End Struct: lwt_info_struct ************************************************/

/* Typedef *******************************************************************/
typedef struct lwt_channel* lwt_chan_t;
typedef struct lwt_info_struct* lwt_t;
typedef struct lwt_channel_group* lwt_cgrp_t;
/* End Typedef ***************************************************************/

/* Public C Function Prototypes **********************************************/
/*****************************************************************************/
void        LWT_INLINE __lwt_schedule(lwt_t thd);
void        LWT_INLINE __lwt_trampoline(void);
void*       __lwt_stack_get(void);
lwt_t       lwt_create(lwt_fn_t fn, void* data);
void        __lwt_destroy(lwt_t thd);
void        lwt_die(void* data);
s32         lwt_yield(lwt_t thd);
lwt_t       lwt_current(void);
s32         lwt_id(lwt_t thd);
s32         lwt_info(lwt_info_t t);
void        __lwt_start(void);
lwt_chan_t  lwt_chan(s32 sz);
void        lwt_chan_deref(lwt_chan_t c);
s32         lwt_snd(lwt_chan_t c,void *data);
void*       lwt_rcv(lwt_chan_t c);
s32         lwt_snd_chan(lwt_chan_t c, lwt_chan_t sending);
lwt_chan_t  lwt_rcv_chan(lwt_chan_t c);
lwt_t       lwt_create_chan(lwt_fn_t fn, lwt_chan_t c);
/*****************************************************************************/
/* End Public C Function Prototypes ******************************************/




#endif /* LWT_H_ */




