/**
 * \file
 * \brief Barrelfish library initialization.
 */

/*
 * Copyright (c) 2007-2019, ETH Zurich.
 * Copyright (c) 2014, HP Labs.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, CAB F.78, Universitaetstr. 6, CH-8092 Zurich,
 * Attn: Systems Group.
 */

#include <stdio.h>

#include <aos/aos.h>
#include <aos/default_interfaces.h>
#include <aos/dispatcher_rpc.h>
#include <aos/dispatch.h>
#include <aos/curdispatcher_arch.h>
#include <aos/dispatcher_arch.h>
#include <barrelfish_kpi/dispatcher_shared.h>
#include <aos/morecore.h>
#include <aos/paging.h>
#include <aos/systime.h>
#include <barrelfish_kpi/domain_params.h>

#include "threads_priv.h"
#include "init.h"
// added
#include <aos/aos_rpc.h>

/// Are we the init domain (and thus need to take some special paths)?
static bool init_domain;


static size_t count = 0;
static struct aos_rpc rpcs[100];



extern size_t (*_libc_terminal_read_func)(char *, size_t);
extern size_t (*_libc_terminal_write_func)(const char *, size_t);
extern void (*_libc_exit_func)(int);
extern void (*_libc_assert_func)(const char *, const char *, const char *, int);

void libc_exit(int);

__weak_reference(libc_exit, _exit);
void libc_exit(int status)
{   


    //TODO dereg from ns
    
    debug_printf("libc exit NYI!\n");
    thread_exit(status);
    // If we're not dead by now, we wait
    while (1) {}
}

static void libc_assert(const char *expression, const char *file,
                        const char *function, int line)
{
    char buf[512];
    size_t len;

    /* Formatting as per suggestion in C99 spec 7.2.1.1 */
    len = snprintf(buf, sizeof(buf), "Assertion failed on core %d in %.*s: %s,"
                   " function %s, file %s, line %d.\n",
                   disp_get_core_id(), DISP_NAME_LEN,
                   disp_name(), expression, function, file, line);
    sys_print(buf, len < sizeof(buf) ? len : sizeof(buf));
}

__attribute__((__used__))
static size_t syscall_terminal_write(const char *buf, size_t len)
{
    if(len) {
        return sys_print(buf, len);
    }
    return 0;
}

__attribute__((__used__))
static size_t syscall_terminal_read(char * buf,size_t len)
{
    sys_getchar(buf);
    return 1;
}

__attribute__((__used__))
static size_t aos_terminal_write(const char * buf,size_t len)
{
  // debug_printf("Terminal write: in aos_terminal_write\n");
    struct aos_rpc * rpc = get_init_rpc();
        if(len) {
            for(size_t i = 0;i < len;++i) {
                aos_rpc_serial_putchar(rpc,buf[i]);
            }
        }
    return len;
}

__attribute__((__used__))
static size_t aos_terminal_read(char* buf,size_t len){
    //debug_printf("Got to aos_terminal_read\n");
    struct aos_rpc * rpc = get_init_rpc();
    errval_t err;
    err = aos_rpc_serial_getchar(rpc, buf);
    if(err_is_fail(err)){
      DEBUG_ERR(err,"Failed get char in aos_terminal_read\n");
    }
    return 1;
}

/**
 * Set libc function pointers
 */
void barrelfish_libc_glue_init(void)
{
    // XXX: FIXME: Check whether we can use the proper kernel serial, and what we need for that
    // TODO: change these to use the user-space serial driver if possible
    // TODO: set these functions
    if (init_domain || true) {
        _libc_terminal_read_func = syscall_terminal_read;
        _libc_terminal_write_func = syscall_terminal_write;
        _libc_exit_func = libc_exit;
        _libc_assert_func = libc_assert;
    } else {
        _libc_terminal_read_func = aos_terminal_read;
        _libc_terminal_write_func = aos_terminal_write;
        _libc_exit_func = libc_exit;
        _libc_assert_func = libc_assert;
    }

    /* morecore func is setup by morecore_init() */

    // XXX: set a static buffer for stdout
    // this avoids an implicit call to malloc() on the first printf
    static char buf[BUFSIZ];
    setvbuf(stdout, buf, _IOLBF, sizeof(buf));
}



/** \brief Initialise libbarrelfish.
 *
 * This runs on a thread in every domain, after the dispatcher is setup but
 * before main() runs.
 */
errval_t barrelfish_init_onthread(struct spawn_domain_params *params)
{
    errval_t err;

    // do we have an environment?
    if (params != NULL && params->envp[0] != NULL) {
        extern char **environ;
        environ = params->envp;
    }

    // Init default waitset for this dispatcher
    struct waitset *default_ws = get_default_waitset();
    waitset_init(default_ws);

    // Initialize ram_alloc state
    ram_alloc_init();

    /* All domains use smallcn to initialize */
    err = ram_alloc_set(ram_alloc_fixed);
    ON_ERR_PUSH_RETURN(err, LIB_ERR_RAM_ALLOC_SET);

    err = paging_init();
    ON_ERR_PUSH_RETURN(err, LIB_ERR_VSPACE_INIT);

    err = slot_alloc_init();
    ON_ERR_PUSH_RETURN(err, LIB_ERR_SLOT_ALLOC_INIT);

    err = morecore_init(BASE_PAGE_SIZE);
    ON_ERR_PUSH_RETURN(err, LIB_ERR_MORECORE_INIT);

    lmp_endpoint_init();

    
    // HINT: Use init_domain to check if we are the init domain.
    if (init_domain) { // init does not need a channel to itself
        err = cap_retype(cap_selfep, cap_dispatcher, 0, ObjType_EndPointLMP, 0, 1);
        ON_ERR_PUSH_RETURN(err, LIB_ERR_CAP_RETYPE);
        set_init_domain();
        return SYS_ERR_OK;
    }

    // Create and init RPC
    err = ram_alloc_set(NULL);
    ON_ERR_RETURN(err);

    err = init_dispatcher_rpcs();
    ON_ERR_RETURN(err);
    // debug_printf("init_dispatcher_rpcs returned\n");
    if(get_ns_online()){
        err = init_nameserver_rpc((char* ) params -> argv[0]);
        ON_ERR_RETURN(err);
    }



    
    // TODO MILESTONE 3: register ourselves with init
    /* allocate lmp channel structure */
    /* create local endpoint */
    /* set remote endpoint to init's endpoint */
    /* set receive handler */
    /* send local ep to init */
    /* wait for init to acknowledge receiving the endpoint */
    /* initialize init RPC client with lmp channel */
    /* set init RPC client in our program state */

    /* TODO MILESTONE 3: now we should have a channel with init set up and can
     * use it for the ram allocator */

    // right now we don't have the nameservice & don't need the terminal
    // and domain spanning, so we return here
    debug_printf("Initialization ok!\n");
    return SYS_ERR_OK;
}


/**
 *  \brief Initialise libbarrelfish, while disabled.
 *
 * This runs on the dispatcher's stack, while disabled, before the dispatcher is
 * setup. We can't call anything that needs to be enabled (ie. cap invocations)
 * or uses threads. This is called from crt0.
 */
void barrelfish_init_disabled(dispatcher_handle_t handle, bool init_dom_arg);
void barrelfish_init_disabled(dispatcher_handle_t handle, bool init_dom_arg)
{
    init_domain = init_dom_arg;
    disp_init_disabled(handle);
    thread_init_disabled(handle, init_dom_arg);
}










void handle_all_binding_request_on_process(struct aos_rpc *r, uintptr_t pid, uintptr_t core_id,uintptr_t client_core ,struct capref client_cap,struct capref * server_cap){
    errval_t err;
    struct capref self_ep_cap = (struct capref) {
        .cnode = cnode_task,
        .slot = TASKCN_SLOT_SELFEP
    };



   
    // debug_printf("got here!\n");
    // struct aos_rpc* rpc = (struct aos_rpc*) malloc(sizeof(struct aos_rpc));
    

    // static struct aos_rpc new_rpc;
    struct aos_rpc * rpc = &(rpcs[count++]);
    initialize_general_purpose_handler(rpc);
    if(client_core == disp_get_current_core_id()){//lmp channel


        struct lmp_endpoint * lmp_ep;
        err = endpoint_create(256,&self_ep_cap,&lmp_ep);
        if(err_is_fail(err)){
            DEBUG_ERR(err,"Failed to create ep in memory server\n");
        }
        err = aos_rpc_init_lmp(rpc,self_ep_cap,client_cap,lmp_ep, NULL);
        if(err_is_fail(err)){
            DEBUG_ERR(err,"Failed to register waitset on rpc\n");
        }

        // char buf[512];
        // debug_print_cap_at_capref(buf,512,self_ep_cap);
        // debug_printf("Cap her %s\n",buf);
        *server_cap = self_ep_cap;
        // debug_print_cap_at_capref(buf,512,*server_cap);
        // debug_printf("Here server cap her %s\n",buf);
    }else {

        char *urpc_data = NULL;
        err = paging_map_frame_complete(get_current_paging_state(), (void **) &urpc_data, client_cap, NULL, NULL);
        if(err_is_fail(err)){
            DEBUG_ERR(err,"Failed to map urpc frame for ump channel into virtual address space\n");
        }
        err =  aos_rpc_init_ump_default(rpc,(lvaddr_t) urpc_data, BASE_PAGE_SIZE,false);//take second half as creating process
        if(err_is_fail(err)){
            DEBUG_ERR(err,"Failed to init_ump_default\n");
        } 
        *server_cap = client_cap; //NOTE: this is fucking stupid
    }



    //err = aos_rpc_init(rpc); // TODO (RPC): set interface
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to init rpc\n");
    }
    
    debug_printf("Channel established on server!\n");
}

/**
 * \brief handler function for send number rpc call
 */
void handle_send_number(struct aos_rpc *r, uintptr_t number) {
    debug_printf("recieved number: %ld\n", number);
}

/**
 * \brief handler function for send string rpc call
 */
void handle_send_string(struct aos_rpc *r, const char *string) {
    debug_printf("recieved string: %s\n", string);
}

void handle_send_varbytes(struct aos_rpc *r, struct aos_rpc_varbytes bytes) {
    debug_printf("recieved bytes (len: %ld): ", bytes.length);
    for (int i = 0; i < bytes.length; i++) {
        debug_printf("%d, ", bytes.bytes[i]);
    }
    debug_printf("\n");
}


void initialize_general_purpose_handler(struct aos_rpc* rpc){
    aos_rpc_register_handler(rpc,AOS_RPC_BINDING_REQUEST,&handle_all_binding_request_on_process);
    aos_rpc_register_handler(rpc, AOS_RPC_SEND_NUMBER, &handle_send_number);
    aos_rpc_register_handler(rpc, AOS_RPC_SEND_STRING, &handle_send_string);
    aos_rpc_register_handler(rpc, AOS_RPC_SEND_VARBYTES, &handle_send_varbytes);
}