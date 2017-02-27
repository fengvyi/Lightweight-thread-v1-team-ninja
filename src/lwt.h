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
/* End Defines ***************************************************************/


/* Config Includes ***********************************************************/
#include "sys_list.h"
#include "lwt_dispatch.h"
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
    INFO_SIZE
} lwt_info_t;
/* End Typedef ***************************************************************/


/* Begin Struct: lwt_info_struct **********************************************
Description : The informations of a thread.
******************************************************************************/
struct lwt_info_struct
{
    struct list_head head;
    s32 tid;
    s32 wait_id;
    lwt_fn_t entry;
    void* init_stack;
    struct lwt_context context;
    void* retval;
    void* data;
    void* sndrcv_data;
    int sndrcv_len;
    int state;
};

struct lwt_channel
{
	int buf_size;
	/* Reference counting - the number of senders and receivers */
	int snd_cnt;
	/* The receiver of the channel */
	struct lwt_info_struct* rcv_data;

	/* The threads that are currently receiving, but nobody have called send */
	struct list_head receiving;
	/* The threads that are currently sending, but nobody have called receive */
	struct list_head sending;
};
/* End Struct: lwt_info_struct ************************************************/

/* Typedef *******************************************************************/
typedef struct lwt_channel* lwt_chan_t;
typedef struct lwt_info_struct* lwt_t;
/* End Typedef ***************************************************************/

/* Public C Function Prototypes **********************************************/
/*****************************************************************************/
void  LWT_INLINE __lwt_schedule(lwt_t thd);
void    __lwt_trampoline(void);
void*   __lwt_stack_get(void);
lwt_t   lwt_create(lwt_fn_t fn, void* data);
void    __lwt_destroy(lwt_t thd);
void*   lwt_join(lwt_t thd);
void    lwt_die(void* data);
int     lwt_yield(lwt_t thd);
lwt_t   lwt_current(void);
int     lwt_id(lwt_t thd);
int     lwt_info(lwt_info_t t);
void    __lwt_start(void);
lwt_chan_t lwt_chan(int sz);
void lwt_chan_deref(lwt_chan_t c);
int lwt_snd(lwt_chan_t c,void *data);
void *lwt_rcv(lwt_chan_t c);
int lwt_snd_chan(lwt_chan_t c, lwt_chan_t sending);
lwt_chan_t lwt_rcv_chan(lwt_chan_t c);
lwt_t lwt_create_chan(lwt_fn_t fn, lwt_chan_t c);
/*****************************************************************************/
/* End Public C Function Prototypes ******************************************/

#endif /* LWT_H_ */




