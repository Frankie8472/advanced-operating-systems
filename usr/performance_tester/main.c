#include <stdio.h>

#include <aos/aos.h>
#include <aos/systime.h>
#include <aos/aos_rpc.h>
#include <aos/default_interfaces.h>


void benchmark_rpc(void);

int main(int argc, char *argv[])
{
    debug_printf("Starting performance measurments\n");

    benchmark_rpc();

    return 0;
}


void benchmark_rpc(void)
{
    const int n_measures = 20;
    uint64_t times[n_measures];
    debug_printf("Testing round-trip-time\n");

    struct aos_rpc *rpc = get_init_rpc();
    for (int i = 0; i < n_measures; i++) {
        uint64_t start = systime_now();
        aos_rpc_call(rpc, AOS_RPC_ROUNDTRIP);
        uint64_t end = systime_now();
        times[i] = end - start;
    }

    uint64_t avg = 0;
    for (int i = 0; i < n_measures; i++) avg += times[i];
    avg /= n_measures;
    debug_printf("Average round-trip-time over %d measurements: %ld [ns]\n", n_measures, systime_to_ns(avg));


    debug_printf("Testing requesting ram\n");

    for (int i = 0; i < n_measures; i++) {
        uint64_t start = systime_now();
        struct capref frame;
        uintptr_t act_size;
        aos_rpc_call(rpc, INIT_IFACE_GET_RAM, 4096, 1, &frame, &act_size);
        uint64_t end = systime_now();
        times[i] = end - start;
        cap_destroy(frame);
    }

    avg = 0;
    for (int i = 0; i < n_measures; i++) avg += times[i];
    avg /= n_measures;
    debug_printf("Average time to request frame of size 4096 over %d measurements: %ld [ns]\n", n_measures, systime_to_ns(avg));
}
