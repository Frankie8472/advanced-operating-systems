#include <stdio.h>
#include <stdlib.h>
#include <aos/aos.h>
#include <aos/aos_rpc.h>
#include <aos/default_interfaces.h>
#include "nameserver_handlers.h"
#include "process_list.h"



void initialize_ns_handlers(struct aos_rpc * init_rpc){
    aos_rpc_register_handler(init_rpc,INIT_REG_NAMESERVER,&handle_reg_proc);
    aos_rpc_register_handler(init_rpc,INIT_REG_INIT,&handle_reg_init);
}


void handle_reg_proc(struct aos_rpc *rpc,uintptr_t core_id,const char* name,struct capref proc_ep_cap, uintptr_t * pid, struct capref* ns_ep_cap){

    

    struct aos_rpc* new_rpc = (struct aos_rpc*) malloc(sizeof(struct aos_rpc));
    errval_t err = add_process(core_id,name,(domainid_t *) pid,new_rpc);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to add process to process list\n");
    }




    if(core_id == disp_get_core_id()){ //create_lmp_channel
        struct capref self_ep_cap = (struct capref) {
        .cnode = cnode_task,
        .slot = TASKCN_SLOT_SELFEP
        };

        struct lmp_endpoint * lmp_ep;
        err = endpoint_create(256,&self_ep_cap,&lmp_ep);
        if(err_is_fail(err)){
            DEBUG_ERR(err,"Failed to create ep in nameserver server\n");
        }
        err = aos_rpc_init_lmp(rpc,self_ep_cap,proc_ep_cap,lmp_ep, NULL);
        if(err_is_fail(err)){
            DEBUG_ERR(err,"Failed to register waitset on nameserver rpc\n");
        }

        *ns_ep_cap = self_ep_cap;
    }else{ //create_ump_channel
        char *urpc_data = NULL;
        err = paging_map_frame_complete(get_current_paging_state(), (void **) &urpc_data, proc_ep_cap, NULL, NULL);
        if(err_is_fail(err)){
            DEBUG_ERR(err,"Failed to map urpc frame for ump channel into virtual address space\n");
        }
        err =  aos_rpc_init_ump_default(rpc,(lvaddr_t) urpc_data, BASE_PAGE_SIZE,true);//take second half as creating process
        if(err_is_fail(err)){
            DEBUG_ERR(err,"Failed to init_ump_default\n");
        } 
        *ns_ep_cap = proc_ep_cap; //NOTE: this is fucking stupid
    }
}


void handle_reg_init(struct aos_rpc *rpc,uintptr_t core_id,const char* name, uintptr_t * pid){
    errval_t err = add_process(core_id,name,(domainid_t *) pid,get_init_rpc());
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to add process to process list\n");
    }
}


