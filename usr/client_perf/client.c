#include <stdio.h>

#include <aos/aos.h>
#include <aos/aos_rpc.h>
#include <aos/waitset.h>
#include <stdio.h>
#include <stdlib.h>
#include <aos/systime.h>
#include <aos/aos.h>
#include <aos/aos_rpc.h>
#include <aos/waitset.h>
#include <aos/paging.h>
#include <aos/nameserver.h>
#define PANIC_IF_FAIL(err, msg)    \
    if (err_is_fail(err)) {        \
        USER_PANIC_ERR(err, msg);  \
    }



#define INTERVAL 1000
static char *myrequest = "request !!";

int main(int argc, char *argv[])
{
    

    errval_t err;


    // lmp_ep_send()
    // debug_printf("Here!\n");
    // uint64_t start = systime_to_ns(systime_now());
    // lmp_ep_send0(get_init_rpc() -> channel.lmp.remote_cap,LMP_FLAG_SYNC,NULL_CAP);
    // uint64_t end = systime_to_ns(systime_now());
    // uint64_t  time  = (end - start) ;
    // debug_printf("%lu\n",time);
    // while(true){
    //     uint64_t start = systime_to_ns(systime_now());
    //     for (int i = 0; i < INTERVAL;++i){
    //         err = aos_rpc_call(get_init_rpc(),AOS_RPC_SEND_STRING,"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n");
    //     }
    //     uint64_t end = systime_to_ns(systime_now());
    //     uint64_t  time  = (end - start) / INTERVAL;
    //     debug_printf("%lu\n",time);

    // }

    // if(argc != 2){
    //     return 1;
    // }
    nameservice_chan_t chan;
    // debug_printf("%s\n",argv[1]);
    // uint64_t start = systime_to_ns(systime_now());
    err = nameservice_lookup(argv[1],&chan);
    PANIC_IF_FAIL(err, "failed to register lookup...\n");
    // uint64_t end = systime_to_ns(systime_now());
    // uint64_t tts = end - start;
    // debug_printf("%lu\n",tts);
    size_t request_size = strlen(myrequest);
    size_t response_bytes;
    void *response;

    uint64_t count = 0;
    uint64_t sum = 0;
    while(true){
        uint64_t start = systime_to_ns(systime_now());

        err = nameservice_rpc(chan, myrequest, request_size,
                        &response, &response_bytes,
                        NULL_CAP, NULL_CAP);
        uint64_t end = systime_to_ns(systime_now());
        uint64_t tts = end - start;
        PANIC_IF_FAIL(err,"Failed to coomunicate with server\n");
        assert(!strcmp((char*) response,"reply!!") && "Not correct reply!");
        count+= 1;
        sum += tts;
        // debug_printf("%lu\n",tts);
        if(count % INTERVAL == 0){
            // double time = ((double) sum) /  INTERVAL;
            debug_printf("%lu\n",sum / 1000);
            // debug_printf("%lf\n",time);
            sum = 0;
            // if(disp_get_domain_id() == 8){
            // }
        }
        free(response);
    }



    return EXIT_SUCCESS;
}
