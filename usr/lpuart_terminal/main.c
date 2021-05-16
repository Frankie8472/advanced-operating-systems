#include <aos/aos.h>
#include <aos/aos_rpc.h>
#include <aos/dispatcher_rpc.h>
#include <aos/inthandler.h>
#include <drivers/lpuart.h>
#include <drivers/gic_dist.h>

const int DEVFRAME_ATTRIBUTES = KPI_PAGING_FLAGS_READ
                              | KPI_PAGING_FLAGS_WRITE
                              | KPI_PAGING_FLAGS_NOCACHE;

struct lpuart_s *lpuart_driver_state;
struct capref irq_ep;

static errval_t initialize_driver(void)
{
    errval_t err;

    struct capref devframe = {
        .cnode = cnode_task,
        .slot = TASKCN_SLOT_DEV
    };

    void *device_frame;

    err = paging_map_frame_attr(get_current_paging_state(), &device_frame,
                                get_phys_size(devframe), devframe,
                                DEVFRAME_ATTRIBUTES, NULL, NULL);
    ON_ERR_RETURN(err);

    err = lpuart_init(&lpuart_driver_state, device_frame);
    ON_ERR_RETURN(err);

    return SYS_ERR_OK;
}


static void handle_interrupt(void *arg) {
    char to_get;
    errval_t err;
    while(true) {
        err = lpuart_getchar(lpuart_driver_state, &to_get);
        if (err_is_fail(err)) {
            break;
        }
        //debug_printf("enterrupt %d '%c'\n", to_get, to_get);
        //printf("%c", to_get);
        err = aos_dc_send(&stdout_chan, 1, &to_get);
        //debug_printf("send err %d\n", err);
    }
}


__unused
static errval_t setup_interrupts(void)
{
    errval_t err;

    struct capref gic_devframe = {
        .cnode = cnode_task,
        .slot = TASKCN_SLOT_BOOTINFO
    };

    void *device_frame;

    err = paging_map_frame_attr(get_current_paging_state(), &device_frame,
                                get_phys_size(gic_devframe), gic_devframe,
                                DEVFRAME_ATTRIBUTES, NULL, NULL);
    ON_ERR_RETURN(err);

    struct gic_dist_s *gic_driver_state;
    err = gic_dist_init(&gic_driver_state, device_frame);
    ON_ERR_RETURN(err);

    err = lpuart_enable_interrupt(lpuart_driver_state);
    ON_ERR_RETURN(err);


    err = inthandler_alloc_dest_irq_cap(IMX8X_UART3_INT, &irq_ep);
    ON_ERR_RETURN(err);

    err = inthandler_setup(irq_ep, get_default_waitset(), MKCLOSURE(handle_interrupt, NULL));
    ON_ERR_RETURN(err);

    err = gic_dist_enable_interrupt(gic_driver_state, IMX8X_UART3_INT, 1 << disp_get_core_id(), 5);
    ON_ERR_RETURN(err);

    return SYS_ERR_OK;
}


static void handle_input(void *arg)
{
    errval_t err;
    struct aos_datachan *in = arg;

    char buffer[32];
    size_t received;
    do {
        err = aos_dc_receive_available(in, sizeof buffer, buffer, &received);
        if (err_is_fail(err)) {
            debug_printf("error handling output in lpuart driver\n");
            return;
        }
        for (size_t i = 0; i < received; i++) {
            //debug_printf("char %d '%c'\n", buffer[i], buffer[i]);
            if (buffer[i] == '\n') {
                //debug_printf("crlf\n");
                lpuart_putchar(lpuart_driver_state, '\r');
            }
            lpuart_putchar(lpuart_driver_state, buffer[i]);
        }
    } while (received >= sizeof buffer);

    err = lmp_chan_register_recv(&stdin_chan.channel.lmp, get_default_waitset(), MKCLOSURE(handle_input, arg));
    ON_ERR_NO_RETURN(err);
}


static errval_t setup_stdin(void)
{
    lmp_chan_register_recv(&stdin_chan.channel.lmp, get_default_waitset(), MKCLOSURE(handle_input, &stdin_chan));
    return SYS_ERR_OK;
}


int main(int argc, char **argv)
{
    errval_t err;

    err = initialize_driver();
    if (err_is_fail(err)) {
        debug_printf("Fatal Error, could not initialize LPUART driver\n");
        abort();
    }

    err = setup_interrupts();
    if (err_is_fail(err)) {
        debug_printf("Fatal Error, could not setup interrupts\n");
        abort();
    }

    err = setup_stdin();


    while (true) {
        err = event_dispatch(get_default_waitset());
        if (err_is_fail(err)) {
            DEBUG_ERR(err, "in event_dispatch");
            abort();
        }
    }
}