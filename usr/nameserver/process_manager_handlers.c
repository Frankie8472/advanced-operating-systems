#include "process_manager_handlers.h"
#include <stdio.h>
#include <stdlib.h>
#include <aos/aos.h>
#include <aos/aos_rpc.h>
#include <aos/default_interfaces.h>
#include "nameserver_handlers.h"
#include "process_list.h"
#include "server_list.h"
#include "nameserver_handlers.h"

extern void *name_server_rpc_handlers[NS_IFACE_N_FUNCTIONS];

void handle_reg_proc(struct aos_rpc *rpc,uintptr_t core_id,const char* name,struct capref proc_ep_cap, uintptr_t pid, struct capref* ns_ep_cap){

    
    errval_t err;
    struct aos_rpc* new_rpc = (struct aos_rpc*) malloc(sizeof(struct aos_rpc));
    err = add_process(core_id,name,(domainid_t )pid,new_rpc);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to add process to process list\n");
    }

    if(core_id == disp_get_core_id()){ //create_lmp_channel
        struct capref new_ep_cap;
        err = slot_alloc(&new_ep_cap);
        if(err_is_fail(err)){
            DEBUG_ERR(err,"Failed slot alloc in nameserver!\n");
        }

        struct lmp_endpoint * lmp_ep;
        err = endpoint_create(256,&new_ep_cap,&lmp_ep);
        if(err_is_fail(err)){
            DEBUG_ERR(err,"Failed to create ep in nameserver server\n");
        }
        err = aos_rpc_init_lmp(new_rpc,new_ep_cap,proc_ep_cap,lmp_ep, get_default_waitset());
        if(err_is_fail(err)){
            DEBUG_ERR(err,"Failed to register waitset on nameserver rpc\n");
        }
        *ns_ep_cap = new_ep_cap;
    }else{ //create_ump_channel
        char *urpc_data = NULL;
        err = paging_map_frame_complete(get_current_paging_state(), (void **) &urpc_data, proc_ep_cap, NULL, NULL);
        if(err_is_fail(err)){
            DEBUG_ERR(err,"Failed to map urpc frame for ump channel into virtual address space\n");
        }
        err =  aos_rpc_init_ump_default(new_rpc,(lvaddr_t) urpc_data, BASE_PAGE_SIZE,false);//take second half as nameserver
        if(err_is_fail(err)){
            DEBUG_ERR(err,"Failed to init_ump_default\n");
        } 
        *ns_ep_cap = proc_ep_cap; //NOTE: this is fucking stupid
    }

    err = aos_rpc_set_interface(new_rpc,get_nameserver_interface(),NS_IFACE_N_FUNCTIONS,name_server_rpc_handlers);
    initialize_nameservice_handlers(new_rpc);
}



void handle_get_proc_name(struct aos_rpc *rpc, uintptr_t pid,char* name){
    for(struct process * curr = pl.head; curr != NULL; curr = curr -> next){
        if(curr -> pid == pid){
            size_t n = strlen(curr -> name) + 1;
            for(int i = 0; i < n;++i ){
                name[i] = curr -> name[i];
            }
            // debug_printf("sending: %s\n",name);
            return;
        }
    }
    debug_printf("could not resolve pid name lookup!\n");
}



void handle_get_proc_core(struct aos_rpc* rpc, uintptr_t pid,uintptr_t *core){

    errval_t err =  get_core_id(pid,(coreid_t*)core);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to find core id form domain %d\n",pid);
        *core = -1;
    }
}



void handle_get_proc_list(struct aos_rpc *rpc, uintptr_t *size,char * pids){
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


void handle_pid_request(struct aos_rpc *rpc,uintptr_t* pid){
    *pid = process++;
}

void handle_dereg_process(struct aos_rpc * rpc, uintptr_t pid){
    errval_t err = remove_process_by_pid(rpc,pid);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to deret process!\n");
    }

    //print_process_list();
}