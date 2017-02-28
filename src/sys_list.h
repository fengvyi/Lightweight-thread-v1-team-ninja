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



/* Begin Function:Sys_create_list *********************************************
Description : Create a doubly linkled list.
Input       : struct List_Head* Head - The pointer to the list head.
Output      : None.
Return      : None.
******************************************************************************/
static void LWT_INLINE sys_create_list(struct list_head* Head)
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
static void LWT_INLINE sys_delete_node(struct list_head* prev,struct list_head* next)
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
static void LWT_INLINE sys_insert_node(struct list_head* new_node,struct list_head* prev,struct list_head* next)
{
    next->prev=new_node;
    new_node->next=next;
    new_node->prev=prev;
    prev->next=new_node;
}
/* End Function:Sys_List_Insert_Node *****************************************/

#endif /* SYS_LIST_H_ */
