/*
 ============================================================================
 Name        : sys_list.c
 Author      : ZF Song, RY Pan
 Version     : Created on: Feb 6, 2017
 Copyright   : Ninja
 Description : The implementation of sys_list.h.
 ============================================================================
 */

/* Config Includes ***********************************************************/
#include "sys_list.h"
/* End Config Includes *******************************************************/


/* Begin Function:Sys_init_list *********************************************
Description : Initialize doubly-linked list glb_head, wait_head, zombie_head.
Input       : None.
Output      : None.
Return      : None.
******************************************************************************/
void
Sys_init_list()
{
	    glb_head.prev=&glb_head;
		glb_head.next=&glb_head;

		wait_head.prev=&wait_head;
		wait_head.next=&wait_head;

		zombie_head.prev=&zombie_head;
		zombie_head.next=&zombie_head;

		pool_head.prev=&pool_head;
		pool_head.next=&pool_head;
}
/* End Function:Sys_init_list **********************************************/


/* Begin Function:Sys_create_list *********************************************
Description : Create a doubly linkled list.
Input       : struct List_Head* Head - The pointer to the list head.
Output      : None.
Return      : None.
******************************************************************************/
void
Sys_create_list(struct list_head* Head)
{
	Head->prev=Head;
	Head->next=Head;
}
/* End Function:Sys_create_list **********************************************/

/* Begin Function:Sys_delete_node ****************************************
Description : Delete a node from the doubly-linked list.
Input       : struct List_Head* prev - The prevoius node of the target node.
              struct List_Head* next - The next node of the target node.
Output      : None.
Return      : None.
******************************************************************************/
void
Sys_delete_node(struct list_head* prev,struct list_head* next)
{
	next->prev=prev;
	prev->next=next;
}
/* End Function:Sys_delete_node *****************************************/



/* Begin Function:Sys_insert_node ****************************************
Description : Insert a node to the doubly-linked list.
Input       : struct List_Head* new_node - The new node to insert.
			  struct List_Head* prev - The previous node.
			  struct List_Head* next - The next node.
Output      : None.
Return      : None.
******************************************************************************/
void
Sys_insert_node(struct list_head* new_node,struct list_head* prev,struct list_head* next)
{
	next->prev=new_node;
	new_node->next=next;
	new_node->prev=prev;
	prev->next=new_node;
}
/* End Function:Sys_List_Insert_Node *****************************************/








