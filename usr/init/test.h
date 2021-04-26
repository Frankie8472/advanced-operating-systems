// macro to put at start of each test-function:
#define TEST_START debug_printf("======================\n");\
    debug_printf("RUNNING TEST: %s\n", __func__);\
    debug_printf("======================\n");

int run_init_tests(int core_id);
