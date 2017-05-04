/*
 ============================================================================
 Name        : sys_list.c
 Author      : ZF Song, RY Pan
 Version     : Created on: Feb 9, 2017
 Copyright   : Ninja
 Description : debug.h.
 ============================================================================
 */

/* Defines *******************************************************************/
#ifndef DEBUG_H_
#define DEBUG_H_
#define LWT_USE_POOL
/* End Defines ***************************************************************/

/* Config Includes ***********************************************************/
#include "lwt.h"
/* End Config Includes *******************************************************/

/* Public C Function Prototypes **********************************************/
/*****************************************************************************/
void  lwt_destroy_by_condition(lwt_t thd);
lwt_t lwt_create_by_condition(lwt_t info,lwt_fn_t fn);
/*****************************************************************************/
/* End Public C Function Prototypes ******************************************/



#endif /* DEBUG_H_ */
