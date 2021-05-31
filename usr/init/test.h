#ifndef INIT_TEST_H_
#define INIT_TEST_H_


// macro to put at start of each test-function:
#define TEST_START debug_printf("======================\n");\
    debug_printf("RUNNING TEST: %s\n", __func__);\
    debug_printf("======================\n");

int run_init_tests(int core_id);



void run_ns_perf_test(coreid_t server_core, uint64_t spawn_interval);

#endif // INIT_TEST_H_
