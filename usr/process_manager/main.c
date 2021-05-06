#include <stdio.h>
#include <stdlib.h>


#include <aos/aos.h>
#include <aos/aos_rpc.h>

// #include <aos/systime.h>
// #include <aos/aos_rpc.h>


struct process_list pl;
domainid_t process;

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
    strncpy(new_str, str, n);
    /*for(size_t i = 0; i < n;++i){
        new_str[i] = str[i];
    }*/

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

static errval_t add_process(coreid_t core_id,const char* name,domainid_t *pid){
    

    struct process * p = (struct process * ) malloc(sizeof(struct process));
    *pid = process++;
    p -> next = NULL,
    p -> pid = *pid;
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



static void handle_register_process(struct aos_rpc *rpc,uintptr_t core_id,const char* name,uintptr_t * pid){
    // debug_printf("Handling process registering! nam: %s\n", name);
    
    errval_t err = add_process(core_id,name,(domainid_t *) pid);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to add process to process list\n");
    }
    // *pid = new_pid;
    // debug_printf("Finished!\n");
    print_process_list();
    return;

}


static void handle_get_proc_name(struct aos_rpc *rpc,uintptr_t pid, char *name){
    // debug_printf("get process name of pid:%d\n",pid);
    
    for(struct process * curr = pl.head; curr != NULL; curr = curr -> next){
        if(curr -> pid == pid){
            size_t n = strlen(curr -> name) + 1;
            // debug_printf("her is n:%d\n",n);
            for(int i = 0; i < n;++i ){
                // debug_printf("copying %c\n",curr -> name[i]);
                name[i] = curr -> name[i];
            }
            // debug_printf("Sending back pname of %s\n",name);
            return;
        }
    }
    // debug_printf("could not resolve pid name lookup!\n");

}


static void handle_get_proc_list(struct aos_rpc *rpc, uintptr_t *size,char * pids){
    // debug_printf("Handle get list of processes\n");
    *size = pl.size;
    char buffer[12]; 
    size_t index = 0;
    for(struct process * curr = pl.head; curr != NULL; curr = curr -> next){
        sprintf(buffer,"%d",curr -> pid);
        // debug_printf("here is buf %s", buffer);
        char * b_ptr = buffer;
        while(*b_ptr != '\0'){
            pids[index] =  *b_ptr;
            index++;
            b_ptr++;
            if(index > 1021){
                debug_printf("Buffer in channels is not large enough to sned full pid list!\n");
                pids[index] = '\0';
                return;
            }
        }

        pids[index] = ',';
        index++;
    }

    pids[index] = '\0';
    // debug_printf("%s\n",pids);
}

static void handle_get_proc_core(struct aos_rpc* rpc, uintptr_t pid,uintptr_t *core){
    for(struct process* curr = pl.head; curr!= NULL; curr = curr ->  next){
        if(curr -> pid == pid){
            *core = curr -> core_id;
            // debug_printf("Sending back core %d\n",*core);
            return;
        }
    }

    *core = -1;
    debug_printf("Could not find pid!\n");
}


static void register_proc_man_handler(struct aos_rpc * init_rpc){
    aos_rpc_register_handler(init_rpc,AOS_RPC_REGISTER_PROCESS,&handle_register_process);
    aos_rpc_register_handler(init_rpc,AOS_RPC_GET_PROC_NAME,&handle_get_proc_name);
    aos_rpc_register_handler(init_rpc,AOS_RPC_GET_PROC_LIST,&handle_get_proc_list);
    aos_rpc_register_handler(init_rpc,AOS_RPC_GET_PROC_CORE,&handle_get_proc_core);
}



int main(int argc, char *argv[])
{   




    char * test = (char * ) malloc(sizeof(char));
    test[0] = 'A';
    load_stack(10);

    errval_t err;
    struct aos_rpc * init_rpc = get_init_rpc();
    register_proc_man_handler(init_rpc);

    debug_printf("Starting process manager\n");
    
    pl.head = NULL;
    pl.tail = NULL;
    pl.size = 0;

    process = 0;
    domainid_t my_pid;
    add_process(0,"process_manager",&my_pid);
    disp_set_domain_id(my_pid);
    
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
