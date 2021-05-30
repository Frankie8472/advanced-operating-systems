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


int main(int argc, char *argv[])
{
    errval_t err;

    const int n_bufnames = 64;
    char **servicenames = malloc(n_bufnames * sizeof(char *));
    size_t n_services;
    err = nameservice_enumerate_with_props("/", "type=mandel", &n_services, servicenames);

    for (size_t i = 0; i < n_services; i++) {
        printf("servicenames[%d]: %s\n", i, servicenames[i]);
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
