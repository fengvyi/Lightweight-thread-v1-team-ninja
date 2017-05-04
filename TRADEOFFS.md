The trade-offs:

1. implementation of the remote&local operations
   There are generally 3 possible ways of implementation:
   1. implement the doubly-linked list atomic operations
      This implementation requires DCAS operations, which is of theoretical value 
      but in circuit implementation works poorly. DCAS refers to "doing CAS on
      two variables" simutaneously.
      int dcas(int* value1, int old1, int new1, int* value2, int old2, int new2)
      {
          if((*value1==old1)&&(*value2==old2))
          {
              *value1=new1;
              *value2=new2;
              return 1;
          }
          else
              return 0;
      }
      This is not to be confused with the double-width CAS.
      Even if such operations are provided, the implementation still does not guarantee
      wait-free operation.(This is a lock-free implementation).

   2. Change all the list structures to a singly linked list implementation.
      This implementation does not require a DCAS, but it cannot avoid the ABA problem
      effectly, and is not wait-free. Additionally, this will require modification on
      the overall system structure, and is not a feasible solution in quite a short
      period of time.

   3. implement a generic multiple-reader multiple-writer queue implementation.
      This implementation uses FAA and is wait-free. However, after evaluating the 
      difficulty of such implementation, this design technique is negated. Noticing
      the fact that 
      1. We only have one reader and possibly multiple writers, 
      2. The number of possible operations in a queue is limited,
      We do not actually need such an implementation to finish all the operations.

   4. implement a single-reader, multiple-writer queue implementation.
      This implementation uses FAA, and is guaranteed to be wait-free. We chose this 
      implementation for its simplicity and efficiency.
      

2. Stack allocation problems
   1. The stack allocation required the stack to contain the thread ID. We can
      1. Allocate the stack from the kernel thread stack frame.
      2. Allocate the stack separately and use a lwt_thdid_set function to set
         the thread ID separately.
      We chose the second implementation, which makes it easier to implement larger
      stacks.
3. Memory management problems
   1. There are two solutions to this problem. The first solution is to allocate 
      memory blocks without freeing it; The second solution is to port the memory 
      management from xv6. we chose the first implementation to test out all the 
      features, and implement the stack management later.
   
3. Comparison between goroutine and lwt-cos implementation
   1. Similarities
      1. The lwt threading library uses a N:M implementation, similiar to that of
         goroutine. Such implementation have the benefit of both worlds, and is very 
         efficient.
      2. The lwt-cos's scheduler is at user space, and the for goroutine, it is the 
         same.
      3. Both libraries, at one kernel thread level, is not preemptive. It requires
         one of the routines to give up CPU explicitly before the other thread to gain
         access to the CPU.
      4. The goroutine's mechanism in channels and channel groups is similar to that
         of lwt.
   2. Differences
      1. In the lwt implementation, if one of the lwt threads calls a system call,
         then all the threads on the lwt will be blocked. For goroutine, however,
         the system will find another kernel thread to run the threads that is still
         runnable.


