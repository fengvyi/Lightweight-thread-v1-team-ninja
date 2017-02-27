/*
 ============================================================================
 Name        : sys_list.h
 Author      : ZF Song, RY Pan
 Version     : Created on: Feb 6, 2017
 Copyright   : Ninja
 Description : The definition of Struct list_head and doubly linked list
 	 	 	   operations.
 ============================================================================
 */



/* Defines *******************************************************************/
#ifndef SYS_LIST_H_
#define SYS_LIST_H_
/* End Defines ***************************************************************/

/* Begin Struct: list_head ****************************************************
Description : The classical doubly linked list head from Linux
Referred to : kernel.c,kernel.h
******************************************************************************/
struct list_head
{
    struct list_head* prev;
    struct list_head* next;
};
/* End Struct: list_head *****************************************************/


/* Public Global Variables ***************************************************/
struct list_head glb_head;
struct list_head wait_head;
struct list_head zombie_head;
struct list_head pool_head;
/* End Public Global Variables ***********************************************/

/* Public C Function Prototypes **********************************************/
/*****************************************************************************/
void sys_init_list();
void sys_create_list(struct list_head* Head);
void sys_delete_node(struct list_head* prev,struct list_head* next);
void sys_insert_node(struct list_head* new_node,struct list_head* prev,struct list_head* next);
/*****************************************************************************/
/* End Public C Function Prototypes ******************************************/

#endif /* SYS_LIST_H_ */
