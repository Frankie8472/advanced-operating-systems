#include <stdio.h>
#include <stdlib.h>

#include <aos/aos.h>
#include <aos/morecore.h>
#include <aos/paging.h>
#include <aos/waitset.h>
#include <aos/aos_rpc.h>
#include <mm/mm.h>
#include <grading.h>
#include <aos/core_state.h>
#include <aos/systime.h>
#include <aos/deferred.h>
#include <aos/waitset.h>


#include <spawn/spawn.h>
#include <spawn/process_manager.h>
#include "mem_alloc.h"
#include "rpc_server.h"

#include "spawn_server.h"
#include "test.h"

int test_stack_overflow(void);


/**
 * \brief causes a stack-overflow by continuously allocating stack-space, then
 * calling itself again
 * \return does not return
 */
__attribute__((noreturn,unused))
int test_stack_overflow(void) {
    char c[1024];
    c[0] = 'a';

    /* debug_printf("Stack address: %ld\n",c); */
    test_stack_overflow();
}

int test_infinite_loop(void);
/**
 * \brief tests the limits memory allocation:
 * indefenitely allocates memory (and maybe frees it)
 * \return does not return
 */
__attribute__((noreturn,unused))
int test_infinite_loop(void){
    /*char* onechar = malloc(1);
    struct paging_state *st = get_current_paging_state();
    for (int i = 0; i < 10000; i++) {
        paging_map_single_page_at(st, (lvaddr_t) onechar + BASE_PAGE_SIZE * i, VREGION_FLAGS_READ_WRITE);
        if (i % 32 == 0) {
            debug_printf("preallocating %d\n", i);
        }
    }*/
    uint64_t count = 0;
    while (true) {
        size_t size = 1L << 20;

        char* p = malloc(size * sizeof(char));
        for(int i = 0; i < size; i += BASE_PAGE_SIZE){
            p[i] = (char) i;
        }
        // free(p);
        if(count % 1 == 0){
            debug_printf("Ran %ld times\n", count);
        }
        count++;
    }
}

int test_printf(void);
/**
 * \brief Tests the printf function by trying to print the target string
 */
int test_printf(void) {
    TEST_START;
    char* tgt = "Test_String";
    debug_printf("trying to print: \"%s\"\n", tgt);
    printf(tgt);
    printf("\n");
    return 0;
}

int test_getchar(void);
/**
 * \brief Tests the getchar function: prompts user to enter various
 * characters, fails if the result is not as requested.
 */
int test_getchar(void) {
    TEST_START;
    for (char t = 'a'; t < 'h'; t++) {
        debug_printf("please enter the char '%c'", t);
        char entered = getchar();

        if (entered == t) {
            debug_printf(" - k thx\n");
            continue;
        }

        debug_printf("ERROR: requested was '%c',"
                     "but getchar() returned '%c'\n", t, entered);
        return 1;
    }
    return 0;
}

int test_malloc(void);
/**
 * \brief Tests malloc functionality: allocates memory up to target size,
 * uses it, then frees it.
 */
int test_malloc(void) {
    TEST_START;
    for (int i = 1024; i <= (1024 * 1024); i *= 2) {
        debug_printf("trying with %d bytes\n", i);
        char* cur = malloc(i * sizeof(char));

        // write to memory
        for (int j = 0; j < i; j++) {
            cur[j] = j % 255;
        }

        // verify results
        for (int j = 0; j < i; j++) {
            if (cur[j] != j % 255) {
                debug_printf("ERROR: index %d should be %d but is %d",
                             j, j % 255, cur[j]);
                free(cur);
                return 1;
            }
        }

        // free memory
        free(cur);
    }
    return 0;
}

__attribute__((unused))
static int test_malloc_lazy(void) {
    TEST_START;

    // allocate 8 GB of memory
    double *array = malloc(sizeof(double) * 1024L * 1024 * 1024);
    debug_printf("malloc'ed 8 GB of memory\n");


    debug_printf("Setting some values to pi\n");
    for(size_t i = 10000000; i < 10005000; i++) {
        array[i] = 3.14159;
    }

    debug_printf("array[10000600] = %lf\n", array[10000600]);

    free(array);

    return 0;
}

__attribute__((unused))
static int test_malloc_64MiB(void) {
    TEST_START;

    // allocate 64 MiB of memory
    size_t len = 16 * 1024 * 1024;
    int *array = malloc(len);
    debug_printf("malloc'ed 64 MiB of memory\n");


    debug_printf("setting each element to its index value\n");
    for(int i = 0; i < len; i++) {
        array[i] = i;
        if (i % (1024 * 1024) == 0) {
            debug_printf("at index %d\n", i);
        }
    }

    long sum = 0;
    for(int i = 0; i < len; i++) {
        sum += array[i];
    }

    debug_printf("The sum of all numbers from 1 to %d = %ld\n", len, sum);

    free(array);

    return 0;
}

int benchmark_mm(void);
/**
 * \brief Benchmarks mm by doing a lot of calls to ram_alloc.
 */
int benchmark_mm(void)
{   

    errval_t err;
    TEST_START;
    const int nBenches = 5000;

    for (int i = 0; i < 10; i++) {
        uint64_t before = systime_now();
        for (int j = 0; j < nBenches; j++) {
            struct capref thrown_away;
            err  = ram_alloc(&thrown_away, BASE_PAGE_SIZE);
            // debug_printf("Allocated ram!\n");
            if(err_is_fail(err)){
                DEBUG_ERR(err,"Failed to allocate ram in benchmarkmm\n");
            }
        }
        uint64_t end = systime_now();

        debug_printf("measurement %d took: %ld\n", i, systime_to_ns(end - before));
    }
    return 0;

    /*
      struct capref cr = (struct capref) {
      .cnode = cnode_root,
      .slot = 0
      };
    
      for (int i = 0; i < 1000; i++) {
      cr.slot = i;
      char buf[256];
      debug_print_cap_at_capref(buf, 256, cr);
      debug_printf("root slot %d: %s\n", i, buf);
      }
    */

}
int benchmark_ump_strings(void);
int benchmark_ump_strings(void) {
    char *ref = "Chapter one - The boy who lived: Mr and Mrs Dursley, of number four, "
        "Privet Drive, were proud  to  say  that  they  were  perfectly  normal,  thank"
        "  you very much. They were the last people you???d expect to be involved in "
        "anything strange or mysterious, because they just didn???t hold with such nonsense."
        "   Mr  Dursley  was  the  director  of  a  fi  rm  called  Grunnings,  which "
        " made  drills.  He  was  a  big,  beefy  man  with  hardly  any  neck,  although"
        "  he  did  have  a  very  large  moustache.  Mrs  Dursley  was  thin  and  blonde"
        "  and  had  nearly  twice  the  usual amount of neck, which came in very useful "
        "as she spent so much of her time craning over garden fences, spying on the "
        "neighbours. The Dursleys had a small son called Dudley and in their opinion there"
        " was no fi ner boy anywhere.   The  Dursleys  had  everything  they  wanted,  "
        "but  they  also  had a secret, and their greatest fear was that somebody would"
        " discover  it.  They  didn???t  think  they  could  bear  it  if  anyone  found  "
        "out  about  the  Potters.  Mrs  Potter  was  Mrs  Dursley???s";

    char *msg_str = malloc(1024 * sizeof(char));
    memset(msg_str, 0, 1024 * sizeof(char));

    errval_t err;
    uint64_t x[1024];
    uint64_t y[1024];
    int lim = 512;
    for (int i = 0; i < 512; i++) {
        uint64_t before = systime_now();
        err = aos_rpc_send_string(get_core_channel(0), msg_str);
        uint64_t after = systime_now();
        x[i] = i + 1;
        y[i] = systime_to_ns(after - before);

        msg_str[i] = ref[i];

        if (err_is_fail(err)) {
            lim = i;
            break;
        }
        debug_printf("finished iter %d\n", i);
    }
    debug_printf("length,delay\n");
    for (int i = 0; i <= lim; i++) {
        debug_printf("%d,%ld\n", x[i], y[i]);
    }

    return 0;
}

int benchmark_ump_numbers(void);
/**
 * \brief Try sending a bunch of numbers to core 0, print performance info (delay).
 */
int benchmark_ump_numbers(void) {
    errval_t err;
    TEST_START;
    debug_printf("message,delay[ns]\n");
    for (int i = 0; i < 512; i++) {
        uint64_t before = systime_now();
        err = aos_rpc_send_number(get_core_channel(0), i);
        uint64_t after = systime_now();
        if (err_is_fail(err)) {
            return 1;
        }
        debug_printf("%d,%ld\n", i, systime_to_ns(after - before));
    }
    return 0;
}

// put your test functions for core 0 in this array, keep NULL as last element
int (*bsp_tests[])(void) = {
    //&benchmark_mm,
    //&test_printf,
    //&test_getchar,
    //&test_malloc,
    //&test_malloc_lazy,
    //&test_malloc_64MiB,
    //&test_stack_overflow,
    //&test_infinite_loop,
    NULL
};

// put your test functions for the other cores in this array, also keep NULL as last element
int (*app_tests[])(void) = {
    //&test_malloc,
    &benchmark_ump_strings,
    /* &benchmark_ump_numbers, */
    NULL
};

/**
 * \brief Main testing function: calls functions from the `tests` array until
 * a testfunction returns something other than 0, or the array is exhausted.
 * \return 0 if all tests pass, non-zero int otherwise
 */
int run_init_tests(int core_id) {
    debug_printf("===============================\n");
    debug_printf("CORE %d, BEGINNING TESTS IN INIT\n", core_id);

    int (**tests)(void) = core_id ? app_tests : bsp_tests;

    for (int i = 0; tests[i]; i++) {
        debug_printf("running test %d\n", i + 1);
        int out = (*tests[i])();

        if (out) {
            debug_printf("core %d, function %d returned error %d, abort\n", core_id, i, out);
            debug_printf("===============================\n");
            return out;
        }
    }
    debug_printf("CORE %d DONE RUNNING TESTS\n", core_id);
    debug_printf("===============================\n");
    
    return 0;
}



static int counter;
#define SERVER "server_perf /server"
static coreid_t s_core;

static void spawn_next_server(void * arg){
    

    char buffer[512];
    char counter_string[120];
    strcpy(buffer,SERVER);
    sprintf(counter_string,"%u",counter);
    strcat(buffer,counter_string);
    spawn_new_domain(buffer, s_core,NULL,NULL,NULL_CAP,NULL_CAP,NULL_CAP,NULL);
    // debug_printf("Spawned new server -client: %d, %s, %d\n",counter,buffer,s_core);
    counter++;
}
void run_ns_perf_test(coreid_t server_core, uint64_t spawn_interval){
    errval_t err;
    counter = 0;
    coreid_t * core = (coreid_t* ) malloc(sizeof(coreid_t));
    // *core = server_core;
    s_core = server_core;
    struct periodic_event * pe = (struct periodic_event *) malloc(sizeof(struct periodic_event));

    err = periodic_event_create(pe,get_default_waitset(),spawn_interval,MKCLOSURE(spawn_next_server,&core));
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to create periodic event!\n");
    }


}