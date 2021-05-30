#include <stdio.h>

#include <aos/aos.h>
#include <aos/aos_rpc.h>
#include <aos/waitset.h>
#include <stdio.h>
#include <stdlib.h>

#include <aos/aos.h>
#include <aos/aos_rpc.h>
#include <aos/waitset.h>
#include <aos/paging.h>
#include <aos/nameserver.h>
#include <aos/default_interfaces.h>

struct aos_rpc calc_connection;
__unused
static void setup_calc_connection(struct capref frame)
{
    void *shared;
    paging_map_frame_complete(get_current_paging_state(), &shared, frame, NULL, NULL);
    aos_rpc_init_ump_default(&calc_connection, (lvaddr_t) shared, get_phys_size(frame), 0);
    aos_rpc_set_interface(&calc_connection, get_dispatcher_interface(), DISP_IFACE_N_FUNCTIONS, malloc(DISP_IFACE_N_FUNCTIONS * sizeof(void *)));
}

__unused
static void remote_ipi(struct capref ipi_ep)
{
    ump_chan_switch_remote_pinged(&calc_connection.channel.ump, ipi_ep);
}

__unused
static void local_ipi(struct capref *ipi_ep)
{
    struct lmp_endpoint *ep;
    struct capref epcap;
    endpoint_create(LMP_RECV_LENGTH, &epcap, &ep);
    slot_alloc(ipi_ep);
    cap_retype(*ipi_ep, epcap, 0, ObjType_EndPointIPI, 0, 1);
    ump_chan_switch_local_pinged(&calc_connection.channel.ump, ep);
}


int main(int argc, char *argv[])
{
    errval_t err;

    const int n_bufnames = 64;
    char **servicenames = malloc(n_bufnames * sizeof(char *));
    size_t n_services;
    err = nameservice_enumerate_with_props("/", "type=mandel", &n_services, servicenames);

    for (size_t i = 0; i < n_services; i++) {
        printf("servicenames[%d]: %s\n", i, servicenames[i]);

        struct capref frame;
        frame_alloc(&frame, BASE_PAGE_SIZE, NULL);
        setup_calc_connection(frame);

        nameservice_chan_t chan;
        nameservice_lookup(servicenames[i], &chan);

        const char cmd1[] = "set_connection";
        void *buf; size_t buf_size;
        nameservice_rpc(chan, (void *) cmd1, sizeof cmd1, &buf, &buf_size, frame, NULL_CAP);

        debug_printf("calling\n");
        aos_rpc_call(&calc_connection, AOS_RPC_ROUNDTRIP);
        debug_printf("call returned\n");

        struct capref ipi_ep;
        struct capref remote_ipi_ep;
        local_ipi(&ipi_ep);
        slot_alloc(&remote_ipi_ep);

        const char cmd2[] = "set_ipi";
        nameservice_rpc(chan, (void *) cmd2, sizeof cmd2, &buf, &buf_size, ipi_ep, remote_ipi_ep);
        remote_ipi(remote_ipi_ep);

        debug_printf("calling\n");
        aos_rpc_call(&calc_connection, AOS_RPC_ROUNDTRIP);
        debug_printf("call returned\n");
    }

    free(servicenames);
    return EXIT_SUCCESS;


    /*
    nameservice_chan_t chan;
    // err = nameservice_lookup(SERVICE_NAME, &chan);
    err = nameservice_lookup_with_prop("/","type=mandel", &chan);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to lookup service!\n");
    }

    printf("Got the service %p. Sending request '%s'\n", chan, myrequest);

    void *request = myrequest;
    size_t request_size = strlen(myrequest);

    void *response;
    size_t response_bytes;
    err = nameservice_rpc(chan, request, request_size,
                          &response, &response_bytes,
                          NULL_CAP, NULL_CAP);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"failed to do the nameservice rpc\n");
    }

    debug_printf("got response: %s\n", (char *)response);

    size_t num;
    char * ret_string[512];
    err = nameservice_enumerate("/",&num,(char**)ret_string);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to enumerate!\n");
    }
    debug_printf("Got response : %d\n",num);
    for(int i = 0; i < num;++i){
        debug_printf("[%d] = %s\n",i,ret_string[i]);
    }

    // char * name;
    // err = aos_rpc_process_get_name(aos_rpc_get_process_channel(),1,&name);

    // char buffer[512];

    err = nameservice_enumerate_with_props("/","type=ethernet",&num,(char**)ret_string);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to enumerate!\n");
    }
    debug_printf("Got response : %d\n",num);
    for(int i = 0; i < num;++i){
        debug_printf("[%d] = %s\n",i,ret_string[i]);
    }
    */
    return EXIT_SUCCESS;

}
