/*
 ============================================================================
 Name        : sys_list.c
 Author      : ZF Song, RY Pan
 Version     : Created on: Feb 9, 2017
 Copyright   : Ninja
 Description : The implementation of debug.h.
 ============================================================================
 */

/* Config Includes ***********************************************************/
#include "debug.h"
/* End Config Includes *******************************************************/

/* Begin Function:lwt_destroy_by_condition**************************************
Description : Destroy a thread, free the memory or put it into pool_head so that
              that thread can be reused quickly for new lwt create calls.
Input       : lwt_t thd - thread we want to destroy.
Output      : None.
Return      : None.
*******************************************************************************/
void
lwt_destroy_by_condition(lwt_t thd)
{
#ifdef LWT_USE_POOL

	sys_insert_node(&(thd->head),pool_head.prev,&pool_head);

#else
	free(thd->init_stack);
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
#ifdef LWT_USE_POOL

	if(pool_head.next!=&pool_head){

		info=list_to_lwt_t(pool_head.next);

		sys_delete_node(&pool_head,pool_head.next->next);

		info->entry=fn;

	}else{

	info=malloc(sizeof(struct lwt_info_struct));

	/* TODO:check if info allocation is successful */

	info->entry=fn;

	/* TODO:check if stack allocation is successful */

	info->init_stack=__lwt_stack_get();

	}

	return info;
#else

    info=malloc(sizeof(struct lwt_info_struct));

    /* TODO:check if info allocation is successful */

		info->entry=fn;

    /* TODO:check if stack allocation is successful */

		info->init_stack=__lwt_stack_get();

    return info;

#endif

}
/* End Function: lwt_create_by_condition****************************************/
