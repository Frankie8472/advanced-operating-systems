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

    struct capref devframe = {
        .cnode = cnode_root,
        .slot = TASKCN_SLOT_DEV
    };

    void *device_frame;

    err = paging_map_frame_attr(get_current_paging_state(), &device_frame,
                                get_phys_size(devframe), devframe,
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
        debug_printf("ERROR: %lu", err);
    }
    debug_printf(">> REACHED\n");

}