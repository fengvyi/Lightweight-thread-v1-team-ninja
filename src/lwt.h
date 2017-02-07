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
#define LWT_NULL 0
#define SIZE_OF_LWT_INFO 3
#define LWT_USE_POOL
#define LWT_INLINE __attribute__((always_inline))
/* End Defines ***************************************************************/


/* Config Includes ***********************************************************/
#include "sys_list.h"
#include "lwt_dispatch.h"
/* End Config Includes *******************************************************/

/* Typedef *******************************************************************/
typedef void(*lwt_fn_t)(void* data);

typedef enum{
	LWT_INFO_NTHD_RUNNABLE = 0,
	LWT_INFO_NTHD_BLOCKED  = 1,
	LWT_INFO_NTHD_ZOMBIES  = 2
  } lwt_info_t;
/* End Typedef ***************************************************************/


/* Begin Struct: lwt_info_struct **********************************************
Description : The informations of a thread.
******************************************************************************/
struct lwt_info_struct
{
	struct list_head head;
	int tid;
	int wait_id;
	lwt_fn_t entry;
	void* init_stack;
	struct lwt_context context;
	void* retval;
};
/* End Struct: lwt_info_struct ************************************************/

/* Typedef *******************************************************************/
typedef struct lwt_info_struct* lwt_t;
/* End Typedef ***************************************************************/

/* Public C Function Prototypes **********************************************/
/*****************************************************************************/
void  LWT_INLINE  __lwt_schedule(lwt_t thd);
void  LWT_INLINE  __lwt_trampoline(void);
void* LWT_INLINE  __lwt_stack_get(void);
void  LWT_INLINE  __lwt_stack_return(void);
lwt_t LWT_INLINE  lwt_create(lwt_fn_t fn, void* data);
void  LWT_INLINE  __lwt_destroy(lwt_t thd);
void* LWT_INLINE  lwt_join(lwt_t thd);
void  LWT_INLINE  lwt_die(void* data);
int   LWT_INLINE  lwt_yield(lwt_t thd);
lwt_t LWT_INLINE  lwt_current(void);
int   LWT_INLINE  lwt_id(lwt_t thd);
int   LWT_INLINE  lwt_info(lwt_info_t t);
/*****************************************************************************/
/* End Public C Function Prototypes ******************************************/




