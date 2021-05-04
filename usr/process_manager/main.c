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
    char* name;
    // Status? alive dead etc etc.
};

struct process_list{
    struct process *head;
    struct process *tail;
    size_t size;
};




static char* strcopy(const char* str){
    size_t n = strlen(str) + 1;
    char * new_str = (char * ) malloc( n * sizeof(char));
    for(size_t i = 0; i < n;++i){
        new_str[i] = str[i];
    }

    return new_str;
}


static void load_stack(int load_size){
    if(load_size == 0){
        return;
    }
    else{
        char c[1024];
        c[0] = 1;
        load_stack(load_size -1);
    }
}
// errval_t init_pl(struct process_list*)

static errval_t add_process(domainid_t pid, coreid_t core_id,const char* name){
    

    struct process * p = (struct process * ) malloc(sizeof(struct process));

    p -> next = NULL,
    p -> pid = pid;
    p -> core_id = core_id;
    p -> name = strcopy(name);


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
    for(struct process* curr = pl.head; curr != NULL; curr = curr -> next){
        debug_printf("Pid:   %d  Core:   %d  Name:   %s\n",curr -> pid, curr -> core_id, curr -> name);
    }
}


// static void(void){
//     debug_printf("================ Processes ====================\n");
//     for(struct process* curr = pl.head; curr != NULL; curr = curr -> next){
//         debug_printf("Pid:   %d  Core:   %d  Name:   %s\n",curr -> pid, curr -> core_id, curr -> name);
//     }
// }



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


static void handle_get_proc_name(struct aos_rpc *rpc,uintptr_t pid, uintptr_t *name){
    debug_printf("get process name of pid:%d\n",pid);
    
    for(struct process * curr = pl.head; curr != NULL; curr = curr -> next){
        debug_printf("%d\n", curr -> pid);
        if(curr -> pid == pid){
            size_t n = strlen(curr -> name) + 1;
            for(int i = 0; i < n;++i ){
                debug_printf("name 0x%lx]\n",name);
                // debug_printf("curr -> name 0x%lx",curr -> name);
                name[i] = (curr -> name)[i];
            }
            // name = (uintptr_t *) curr -> name; 
            return;
        }
    }
    debug_printf("could not resolve pid name lookup!\n");

}


int main(int argc, char *argv[])
{   

    char * test = (char * ) malloc(sizeof(char));
    test[0] = 'A';



    load_stack(10);
    errval_t err;
    struct aos_rpc * init_rpc = get_init_rpc();

    err = aos_rpc_call(init_rpc,AOS_RPC_PUTCHAR,'c');
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to send putchar\n");
    }


    debug_printf("Starting process manager\n");
    
    pl.head = NULL;
    pl.tail = NULL;
    pl.size = 0;
    
    aos_rpc_register_handler(init_rpc,AOS_RPC_REGISTER_PROCESS,&handle_register_process);
    aos_rpc_register_handler(init_rpc,AOS_RPC_GET_PROC_NAME,&handle_get_proc_name);

    struct waitset *default_ws = get_default_waitset();

    err = aos_rpc_call(init_rpc,AOS_RPC_SERVICE_ON,0);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to call AOS_RPC_SERVICE_ON\n");
    }

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
