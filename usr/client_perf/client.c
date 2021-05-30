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
    if(argc != 2){
        return 1;
    }
    size_t request_size = strlen(myrequest);
    size_t response_bytes;
    void *response;
    nameservice_chan_t chan;
    err = nameservice_lookup(argv[1],&chan);
    PANIC_IF_FAIL(err, "failed to register lookup...\n");


    uint64_t count = 0;
    uint64_t sum = 0;
    while(true){
        uint64_t start = ns_to_systime(systime_now());

        err = nameservice_rpc(chan, myrequest, request_size,
                        &response, &response_bytes,
                        NULL_CAP, NULL_CAP);
        uint64_t end = ns_to_systime(systime_now());
        uint64_t tts = end - start;
        PANIC_IF_FAIL(err,"Failed to coomunicate with server\n");
        assert(!strcmp((char*) response,"reply!!") && "Not correct reply!");
        count+= 1;
        sum += tts;
        if(count % INTERVAL == 0){
            double time = ((double) sum) /  INTERVAL;
            sum = 0;
            debug_printf("%lf!\n",time);
        }
        free(response);
    }



    return EXIT_SUCCESS;
}
