/*
 * Copyright 2016, Phani Gadepalli and Gabriel Parmer, GWU, gparmer@gwu.edu.
 *
 * This uses a two clause BSD License.
 */

#include <stdio.h>
#include <string.h>
#include <cos_component.h>
#include <cobj_format.h>
#include <cos_defkernel_api.h>

#include <sl.h>
extern void* local_sto[4096];
#include <lwt.h>

/* For LWT Testing */
#define rdtscll(val) __asm__ __volatile__("rdtsc" : "=A" (val))

#define ITER 10000
/* End for LWT Testing */

/* Stuff that we do not care about */
#undef assert
#define assert(node) do { if (unlikely(!(node))) { debug_print("assert error in @ "); *((int *)0) = 0; } } while (0)
#define PRINT_FN prints
#define debug_print(str) (PRINT_FN(str __FILE__ ":" STR(__LINE__) ".\n"))
#define BUG() do { debug_print("BUG @ "); *((int *)0) = 0; } while (0);
#define SPIN(iters) do { if (iters > 0) { for (; iters > 0 ; iters -- ) ; } else { while (1) ; } } while (0)

static void
cos_llprint(char *s, int len)
{ call_cap(PRINT_CAP_TEMP, (int)s, len, 0, 0); }

int
prints(char *s)
{
	int len = strlen(s);

	cos_llprint(s, len);

	return len;
}

int __attribute__((format(printf,1,2)))
printc(char *fmt, ...)
{
	  char s[128];
	  va_list arg_ptr;
	  int ret, len = 128;

	  va_start(arg_ptr, fmt);
	  ret = vsnprintf(s, len, fmt, arg_ptr);
	  va_end(arg_ptr);
	  cos_llprint(s, ret);

	  return ret;
}

#define N_TESTTHDS 8
#define WORKITERS  10000

void
test_thd_fn(void *data)
{
	while (1) {
		int workiters = WORKITERS * ((int)data);

		printc("%d", (int)data);
		SPIN(workiters);
		sl_thd_yield(0);
	}
}

void
test_yields(void)
{
	int                     i;
	struct sl_thd          *threads[N_TESTTHDS];
	union sched_param       sp    = {.c = {.type = SCHEDP_PRIO, .value = 10}};

	for (i = 0 ; i < N_TESTTHDS ; i++) {
		threads[i] = sl_thd_alloc(test_thd_fn, (void *)(i + 1));
		assert(threads[i]);
		sl_thd_param_set(threads[i], sp.v);
	}
}

void
test_high(void *data)
{
	struct sl_thd *t = data;

	while (1) {
		sl_thd_yield(t->thdid);
		printc("h");
	}
}

void
test_low(void *data)
{
	while (1) {
		int workiters = WORKITERS * 10;
		SPIN(workiters);
		printc("l");
	}
}

void
test_blocking_directed_yield(void)
{
	struct sl_thd          *low, *high;
	union sched_param       sph = {.c = {.type = SCHEDP_PRIO, .value = 5}};
	union sched_param       spl = {.c = {.type = SCHEDP_PRIO, .value = 10}};

	low  = sl_thd_alloc(test_low, NULL);
	high = sl_thd_alloc(test_high, low);
	sl_thd_param_set(low, spl.v);
	sl_thd_param_set(high, sph.v);
}














void *
fn_bounce(void *d) 
{
	int i;
	unsigned long long start, end;

	lwt_yield(LWT_NULL);
	lwt_yield(LWT_NULL);
	rdtscll(start);
	for (i = 0 ; i < ITER ; i++) lwt_yield(LWT_NULL);
	rdtscll(end);
	lwt_yield(LWT_NULL);
	lwt_yield(LWT_NULL);

	if (!d) printc("[PERF] %5lld <- yield\n", (end-start)/(ITER*2));

	return NULL;
}


void *
fn_null(void *d)
{ return NULL; }

/*
#define IS_RESET()						\
        assert( lwt_info(LWT_INFO_NTHD_RUNNABLE) == 1 &&	\
		lwt_info(LWT_INFO_NTHD_ZOMBIES) == 0 &&		\
		lwt_info(LWT_INFO_NTHD_BLOCKED) == 0)
*/



void
test_perf(void)
{
	lwt_t chld1, chld2;
	int i;
	unsigned long long start, end;


	// Performance tests 
	rdtscll(start);
	for (i = 0 ; i < ITER ; i++) {
		chld1 = lwt_create(fn_null, NULL);
		lwt_join(chld1);
	}
	rdtscll(end);
	printc("[PERF] %5lld <- fork/join\n", (end-start)/ITER);
	//IS_RESET();

	chld1 = lwt_create(fn_bounce, (void*)1);
	chld2 = lwt_create(fn_bounce, NULL);
	lwt_join(chld1);
	lwt_join(chld2);
	//IS_RESET();
} 




void *
fn_chan(lwt_chan_t to)
{
    printc("fn_chan begin\n");
	lwt_chan_t from;
	int i;
	
	from = lwt_chan(0);
	lwt_snd_chan(to, from);
    printc("chan sent\n");
	assert(from->snd_cnt);
    printc("begin loop\n");
	for (i = 0 ; i < ITER ; i++) {
        //printc("loop2:%d\n",i);
		lwt_snd(to, (void*)1);
        //printc("sent\n");
		assert(2 == (int)lwt_rcv(from));
	}
	lwt_chan_deref(from);
	
	return NULL;
}

void
test_perf_channels(int chsz)
{
    printc("@test_perf_channels begin\n");
	lwt_chan_t from, to;
	lwt_t t;
	int i;
	unsigned long long start, end;

	assert(LWT_RUNNING == lwt_current()->state);
	from = lwt_chan(chsz);
    printc("channel created\n");
	assert(from);
	t    = lwt_create_chan(fn_chan, from);
    printc("fn_chan created\n");
	to   = lwt_rcv_chan(from);
    printc("begin ping pong\n");
	assert(to->snd_cnt);
	rdtscll(start);
	for (i = 0 ; i < ITER ; i++) {
        //printc("loop1:%d\n",i);
		assert(1 == (int)lwt_rcv(from));
        //printc("received\n");
		lwt_snd(to, (void*)2);
	}
    //printc("loop2 end\n");
	lwt_chan_deref(to);
	rdtscll(end);
	printc("[PERF] %5lld <- snd+rcv (buffer size %d)\n", 
	       (end-start)/(ITER*2), chsz);
	lwt_join(t);
    
}



s32 test2(void* data)
{
	s32* myvar=malloc(4);
	*myvar=1234;
    printc("About to snd\n");
	lwt_snd(data,myvar);
    printc("Sent\n");
	return 0;
}

s32 test(void* data)
{
	lwt_chan_t channel=lwt_chan(4);
	lwt_t sender=lwt_create_chan(test2,channel);
    printc("Chan created\n");
	s32* myvar=lwt_rcv(channel);
	printc("I received %d\n",*myvar);
	free(myvar);
	lwt_join(sender);
	exit(0);
}





int func1(void* data)
{
    /* Start all other kthds, and let's start our test */
    printc("Oh, we are testing it\n");
    while(1);
}

/* The first function to run */

void 
test_lwt(void)
{
    /* Start the first thread */
    lwt_kthd_create(test_perf, 0);
    //lwt_kthd_create(test_perf_channels, 0);
   
}

void 
test_kthd_chan(void)
{
    printc("chan test begin\n");
     /* Start the second thread */
    lwt_kthd_create(test_perf_channels, 0);
}

void
test_int_chan(lwt_chan_t c)
{
    lwt_snd(c,(void*)1);
    printc("send complete\n");
    while(1)
    kthd_yield(0);
   
}


int 
test_perf_interkthd_chan(void* data)
{
    printc("About to create channel\n");
    lwt_chan_t channel=lwt_chan(4);
    lwt_kthd_create(test_int_chan,channel);
    assert(1 == (int)lwt_rcv(channel));
    printc("Inter-kthd channel received\n");
}

void 
test_interkthd_chan(void)
{
    lwt_kthd_create(test_perf_interkthd_chan,0);
}

void 
test_grp(void)
{
    printc("Test group begin!\n");
	lwt_chan_t channel=lwt_chan(4);
	lwt_chan_t channel2=lwt_chan(4);
    printc("Channel 1 and 2 created\n");
	lwt_cgrp_t group=lwt_cgrp();
    printc("group created\n");
	assert(0==lwt_cgrp_add(group,channel));
	assert(0==lwt_cgrp_add(group,channel2));
    printc("Channels added to group\n");
	lwt_chan_mark_set(channel,(void*)1);
    printc("mark set\n");
	lwt_t sender=lwt_create_chan(test2,channel);
	//lwt_yield(sender);
	s32* myvar=lwt_rcv(channel);
	printc("I received %d\n",*myvar);
	free(myvar);
	assert(1==lwt_chan_mark_get(channel));
	assert(0==lwt_cgrp_rem(group,channel));
	assert(0==lwt_cgrp_rem(group,channel2));
	printc("retval %d\n",lwt_cgrp_free(group));
}

void 
test_kthd_grp()
{
    lwt_kthd_create(test_grp,0);
}


static int sndrcv_cnt = 0;

void *
fn_snder(lwt_chan_t c, int v)
{
	int i;

	for (i = 0 ; i < ITER ; i++) {
		lwt_snd(c, (void*)v);
		sndrcv_cnt++;
	}

	return NULL;
}

void *fn_snder_1(lwt_chan_t c) { return fn_snder(c, 1); }
void *fn_snder_2(lwt_chan_t c) { return fn_snder(c, 2); }

int
test_multisend(void* data)
{
    int chsz=4;
    printc("test_multisend begin!\n");
    kthd_t kthd=(kthd_t)tls_get(0);
    cos_set_thd_id(stack_frame_get()+4,3);
    printc("multi-send kthd:%d\n",kthd);
	lwt_chan_t c=lwt_chan(4);
	lwt_t t1, t2;
	int i, ret[ITER*2], sum = 0, maxcnt = 0;
	assert(c);
	printc("[TEST] multisend (channel buffer size %d)\n", chsz);

	
	t1 = lwt_create_chan(fn_snder_2, c);
	t2 = lwt_create_chan(fn_snder_1, c);
	for (i = 0 ; i < ITER*2 ; i++) {
		//if (i % 5 == 0) lwt_yield(LWT_NULL);
		ret[i] = (int)lwt_rcv(c);
		if (sndrcv_cnt > maxcnt) maxcnt = sndrcv_cnt;
		sndrcv_cnt--;
	}
	lwt_join(t1);
	lwt_join(t2);
	
	for (i = 0 ; i < ITER*2 ; i++) {
		sum += ret[i];
		assert(ret[i] == 1 || ret[i] == 2);
	}
    while(1)
	assert(sum == (ITER * 1) + (ITER*2));
	/* 
	 * This is important: Asynchronous means that the buffer
	 * should really fill up here as the senders never block until
	 * the buffer is full.  Thus the difference in the number of
	 * sends and the number of receives should vary by the size of
	 * the buffer.  If your implementation doesn't do this, it is
	 * doubtful you are really doing asynchronous communication.
	 */
	assert(maxcnt >= chsz);
   
	return 0;
}

void 
test_kthd_multisend(void)
{
    lwt_kthd_create(test_multisend,0);
}

void
cos_init(void)
{
	struct cos_defcompinfo *defci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci    = cos_compinfo_get(defci);

	printc("Unit-test for the scheduling library (sl)\n");
	cos_meminfo_init(&(ci->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_defcompinfo_init();
	sl_init();
//	test_yields();
//	test_blocking_directed_yield();
    //test_lwt();
    //test_kthd_chan();
    //test_interkthd_chan();
    //test_kthd_grp();
    test_kthd_multisend();

	sl_sched_loop();

	assert(0);

	return;
}
