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

#include "cos_kernel_api.h"
#include <cos_defkernel_api.h>

/* Defines *******************************************************************/
#ifndef SYS_LIST_H_
#define SYS_LIST_H_
/* End Defines ***************************************************************/

typedef unsigned int uint; 
/* memory allocator */
typedef long Align;

union header {
  struct {
    union header *ptr;
    uint size;
  } s;
  Align x;
};

typedef union header Header;

/* The CAS and faa stub */
static inline int
lwt_cas(unsigned long *target, unsigned long old, unsigned long updated)
{
	char z;
	__asm__ __volatile__("lock cmpxchgl %2, %0; setz %1"
			     : "+m" (*target),
			       "=a" (z)
			     : "q"  (updated),
			       "a"  (old)
			     : "memory", "cc");
	return (int)z;
}

/* Multiple-producer single-consumer queue wait-free circular queue */
struct cc_queue
{
    unsigned long head_ptr;
    unsigned long tail_ptr;
    void* buf[1024];
} que;

typedef struct cc_queue* ccq_t;

#define CCQ_ROUND(X) ((X)%1024)

/* Fetch-and-add implementation on x86. It returns the original value
 * before xaddl. */
static inline int
lwt_faa(int *var, int value)
{
	__asm__ __volatile__("lock xaddl %%eax, %2;"
			     :"=a" (value)            //Output
			     :"a" (value), "m" (*var) //Input
			     :"memory");
	return value;
}

/* TLS facilities */
/*
static unsigned long inline
tls_get(size_t off)
{
	unsigned long val;

	__asm__ __volatile__("movl %%gs:(%1), %0" : "=r" (val) : "r" (off) : );

	return val;
}

static void inline
tls_set(size_t off, unsigned long val)
{ __asm__ __volatile__("movl %0, %%gs:(%1)" : : "r" (val), "r" (off) : "memory"); }
*/
static unsigned long inline
tls_get(size_t off)
{
	return local_sto[cos_thdid()];
}

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
