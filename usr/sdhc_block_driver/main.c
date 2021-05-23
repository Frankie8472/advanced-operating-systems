#include <aos/aos.h>
#include <aos/aos_rpc.h>
#include <drivers/sdhc.h>

const int DEVFRAME_ATTRIBUTES = KPI_PAGING_FLAGS_READ
                                | KPI_PAGING_FLAGS_WRITE
                                | KPI_PAGING_FLAGS_NOCACHE;

struct sdhc_s *sdhc_driver_state;

static errval_t initialize_driver(void)
{
    errval_t err;

    struct capref argcn_frame = {
        .cnode = cnode_argcn,
        .slot = ARGCN_SLOT_DEV_0
    };

    void *device_frame;
    err = paging_map_frame_attr(get_current_paging_state(), &device_frame,
                                get_phys_size(argcn_frame), argcn_frame,
                                DEVFRAME_ATTRIBUTES, NULL, NULL);
    ON_ERR_RETURN(err);

    err = sdhc_init(&sdhc_driver_state, device_frame);
    ON_ERR_RETURN(err);

    return SYS_ERR_OK;
}


int main(int argc, char **argv) {
    errval_t err;
    err = initialize_driver();
    if (err_is_fail(err)) {
        debug_printf("Fatal Error, could not initialize SHDC driver\n");
        abort();
    }


    struct capref tmp;
    size_t retbytes;
    void *addr;
    err = frame_alloc_and_map_flags(&tmp, 512, &retbytes, &addr, VREGION_FLAGS_READ_WRITE_NOCACHE);
    if (err_is_fail(err)) {
        debug_printf("ERROR: Frame alloc fail\n");
        abort();
    }

    err = sdhc_test(sdhc_driver_state, addr, get_phys_addr(tmp));
    if (err_is_fail(err)) {
        debug_printf("ERROR: sdhc test fail\n");
        abort();
    }

    // TODO: free by caller
    debug_printf(">> REACHED\n");
}