#include <stdio.h>

#include <aos/aos.h>
#include <aos/aos_rpc.h>

// #include <aos/systime.h>
// #include <aos/aos_rpc.h>


struct process_list pl;

struct process {
    struct process * next;
    domainid_t pid;
    coreid_t core_id;
    const char* name;
    // Status? alive dead etc etc.
};

struct process_list{
    struct process *head;
    struct process *tail;
};

// errval_t init_pl(struct process_list*)

static errval_t add_process(domainid_t pid, coreid_t core_id,const char* name){
    // struct process * p = (struct process *) malloc(sizeof(process));
    struct process p = {
        .next = NULL,
        .pid = pid,
        .core_id = core_id,
        .name = name
    };

    if(pl.head == NULL){
        pl.head = &p;
        pl.tail = &p;
    }else{
        pl.tail -> next = &p;
        pl.tail = &p;
    }

    return SYS_ERR_OK;
};


static void handle_register_process(struct aos_rpc *rpc,uintptr_t pid,uintptr_t core_id,const char* name){
    debug_printf("Handling process registering!\n");
    errval_t err = add_process((domainid_t) pid, (coreid_t) core_id,name);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to add process to process list\n");
    }
    return;

}



int main(int argc, char *argv[])
{

    debug_printf("Starting process manager\n");
    errval_t err;
    struct aos_rpc * init_rpc = get_init_rpc();
    // aos_rpc_init(init_rpc);

    // waitset_init(get_default_waitset());
    aos_rpc_register_handler(init_rpc,AOS_RPC_REGISTER_PROCESS,&handle_register_process);


    struct waitset *default_ws = get_default_waitset();
    // err = lmp_chan_register_recv(&init_rpc->channel.lmp, default_ws, MKCLOSURE(&aos_rpc_on_message, init_rpc));

    // if(err_is_fail(err)){
    //     DEBUG_ERR(err,"Failed to register channel to waitset\n");
    // }


    err = aos_rpc_call(init_rpc,AOS_RPC_PM_ONLINE);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to call AOS_RPC_PM_ONLINE\n");
    }


    debug_printf("Message handler loop\n");
    err = event_dispatch(default_ws);
    while (true) {
        err = event_dispatch(default_ws);
        if (err_is_fail(err)) {
            debug_printf("err in event_dispatch\n");
            //DEBUG_ERR(err, "in event_dispatch");
            abort();
        }
    }

    return 0;
}
