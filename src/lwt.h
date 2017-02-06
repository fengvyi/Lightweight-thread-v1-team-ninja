#define LWT_NULL 0

#define SIZE_OF_LWT_INFO 3
#define LWT_USE_POOL
#define LWT_INLINE __attribute__((always_inline))

#include "lwt_dispatch.h"
typedef void(*lwt_fn_t)(void* data);

typedef enum{LWT_INFO_NTHD_RUNNABLE=0, LWT_INFO_NTHD_BLOCKED=1, LWT_INFO_NTHD_ZOMBIES=2} lwt_info_t;

struct list_head
{
	struct list_head* prev;
	struct list_head* next;
};

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

typedef struct lwt_info_struct* lwt_t;

void LWT_INLINE
__lwt_schedule(lwt_t thd);

void LWT_INLINE
__lwt_trampoline(void);

void* LWT_INLINE
__lwt_stack_get(void);

void LWT_INLINE
__lwt_stack_return(void);

lwt_t LWT_INLINE
lwt_create(lwt_fn_t fn, void* data);


void LWT_INLINE
__lwt_destroy(lwt_t thd);

void* LWT_INLINE
lwt_join(lwt_t thd);


void LWT_INLINE
lwt_die(void* data);

int LWT_INLINE
lwt_yield(lwt_t thd);

lwt_t LWT_INLINE
lwt_current(void);

int LWT_INLINE
lwt_id(lwt_t thd);

int LWT_INLINE
lwt_info(lwt_info_t t);




