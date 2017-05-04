/*
 ============================================================================
 Name        : sys_list.c
 Author      : ZF Song, RY Pan
 Version     : Created on: Feb 9, 2017
 Copyright   : Ninja
 Description : The implementation of debug.h.
 ============================================================================
 */
extern void* local_sto[4096];
/* Config Includes ***********************************************************/
#include "debug.h"
#include "cos_kernel_api.h"
/* End Config Includes *******************************************************/

extern void stkfree(void* stack, kthd_t kthd);
extern void cos_set_thd_id(unsigned long stack_ptr,long thdid);
/* Begin Function:lwt_destroy_by_condition**************************************
Description : Destroy a thread, free the memory or put it into (kthd->pool_head) so that
              that thread can be reused quickly for new lwt create calls.
Input       : lwt_t thd - thread we want to destroy.
Output      : None.
Return      : None.
*******************************************************************************/
void
lwt_destroy_by_condition(lwt_t thd)
{
    /* The current kthd is in the TLS */
    kthd_t kthd=(kthd_t)tls_get(0);
#ifdef LWT_USE_POOL
	sys_insert_node(&(thd->head),(kthd->pool_head).prev,&(kthd->pool_head));
#else
	free(thd->init_stack,kthd);
	free(thd);
#endif

}
/* End Function: lwt_destroy_by_condition****************************************************/

/* Begin Function:lwt_create_by_condition*******************************************
Description : Create a thread from malloc or reuse from pool.
Input       : lwt_fn_t fn, void* data.
Output      : lwt_t info -Thread we create.
Return      : lwt_t info -Thread we create.
*******************************************************************************/
lwt_t
lwt_create_by_condition(lwt_t info,lwt_fn_t fn)
{
    /* The current kthd is in the TLS */
    kthd_t kthd=(kthd_t)tls_get(0);
#ifdef LWT_USE_POOL

	if((kthd->pool_head).next!=&(kthd->pool_head)){
		info=list_to_lwt_t((kthd->pool_head).next);
		sys_delete_node(&(kthd->pool_head),(kthd->pool_head).next->next);
		info->entry=fn;
	}else{

	info=malloc(sizeof(struct lwt_info_struct));

	/* TODO:check if info allocation is successful */

	info->entry=fn;
    
	/* TODO:check if stack allocation is successful */

	info->init_stack=__lwt_stack_get(kthd);
    printc("Thread ID set:%d\n",kthd->kthd_struct->thdid);
    cos_set_thd_id(info->init_stack+4,kthd->kthd_struct->thdid);
	}
    info->kthd=kthd;
	return info;
#else

    info=malloc(sizeof(struct lwt_info_struct));

    /* TODO:check if info allocation is successful */

		info->entry=fn;

    /* TODO:check if stack allocation is successful */

		info->init_stack=__lwt_stack_get(kthd);
    printc("Thread ID set:%d\n",kthd->kthd_struct->thdid);
    cos_set_thd_id(info->init_stack+4,kthd->kthd_struct->thdid);
    info->kthd=kthd;
    return info;

#endif

}

lwt_t
__lwt_create_by_condition(lwt_t info,lwt_fn_t fn, kthd_t kthd)
{
 
#ifdef LWT_USE_POOL

	if((kthd->pool_head).next!=&(kthd->pool_head)){
		info=list_to_lwt_t((kthd->pool_head).next);
		sys_delete_node(&(kthd->pool_head),(kthd->pool_head).next->next);
		info->entry=fn;
	}else{

	info=malloc(sizeof(struct lwt_info_struct));

	/* TODO:check if info allocation is successful */

	info->entry=fn;

	/* TODO:check if stack allocation is successful */

	info->init_stack=__lwt_stack_get(kthd);
    printc("Thread ID set:%d\n",kthd->kthd_struct->thdid);
    cos_set_thd_id(info->init_stack+4,kthd->kthd_struct->thdid);

	}
    info->kthd=kthd;
	return info;
#else

    info=malloc(sizeof(struct lwt_info_struct));

    /* TODO:check if info allocation is successful */

		info->entry=fn;

    /* TODO:check if stack allocation is successful */

		info->init_stack=__lwt_stack_get(kthd);
    printc("Thread ID set:%d\n",kthd->kthd_struct->thdid);
    cos_set_thd_id(info->init_stack+4,kthd->kthd_struct->thdid);
    info->kthd=kthd;
    return info;

#endif

}
/* End Function: lwt_create_by_condition****************************************/
