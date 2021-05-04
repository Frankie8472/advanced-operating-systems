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
    size_t size;
};

// errval_t init_pl(struct process_list*)

static errval_t add_process(domainid_t pid, coreid_t core_id,const char* name){
    

    struct process * p = (struct process * ) malloc(sizeof(struct process));

    p -> next = NULL,
    p -> pid = pid;
    p -> core_id = core_id;
    p -> name = name;
    // struct process p = (struct process) {
    //     .next = NULL,
    //     .pid = pid,
    //     .core_id = core_id,
    //     .name = name,
    // };

    if(pl.head == NULL){
        pl.head = p;
        pl.tail = p;
    }else{
        pl.tail -> next = p;
        pl.tail = p;
    }
    pl.size++;
    return SYS_ERR_OK;
};

static void print_process_list(void){
    debug_printf("================ Processes ====================\n");
    debug_printf("Size: %d\n",pl.size);
    struct process* curr = pl.head;
    for(size_t i = 0; i < pl.size;++i){
        debug_printf("Pid:   %d  Core:   %d  Name:   %s\n",curr -> pid, curr -> core_id, curr -> name);


        curr = curr -> next;
    }
    // for(struct process* curr = pl.head; curr != NULL; curr = curr -> next){
    //     debug_printf("Pid:   %d  Core:   %d  Name:   %s\n",curr -> pid, curr -> core_id, curr -> name);
    // }
}


static void handle_register_process(struct aos_rpc *rpc,uintptr_t pid,uintptr_t core_id,const char* name){
    debug_printf("Handling process registering!\n");
    errval_t err = add_process((domainid_t) pid, (coreid_t) core_id,name);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to add process to process list\n");
    }
    debug_printf("Finished!\n");
    print_process_list();
    return;

}



int main(int argc, char *argv[])
{   

    
    errval_t err;
    struct aos_rpc * init_rpc = get_init_rpc();

    err = aos_rpc_call(init_rpc,AOS_RPC_PUTCHAR,'c');
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Faile to send putchar\n");
    }
    debug_printf("Starting process manager\n");

    
    // aos_rpc_init(init_rpc);
    aos_rpc_register_handler(init_rpc,AOS_RPC_REGISTER_PROCESS,&handle_register_process);

    pl.head = NULL;
    pl.tail = NULL;
    pl.size = 0;
    struct waitset *default_ws = get_default_waitset();


    // err = request_and_map_memory();
    // if(err_is_fail(err)){
    //     DEBUG_ERR(err,"Failed to request and map memory\n");
    // }

    err = aos_rpc_call(init_rpc,AOS_RPC_PM_ONLINE);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to call AOS_RPC_PM_ONLINE\n");
    }

    char * test = (char * ) malloc(sizeof(char));
    test[0] = 'A';

    

    debug_printf("Message handler loop\n");
    while (true) {
        err = event_dispatch(default_ws);
        if (err_is_fail(err)) {
            debug_printf("err in event_dispatch\n");
            abort();
        }
    }

    return 0;
}
