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
#include <aos/io_channels.h>
#include <barrelfish_kpi/domain_params.h>

#include "threads_priv.h"
#include "init.h"
// added
#include <aos/aos_rpc.h>

/// Are we the init domain (and thus need to take some special paths)?
static bool init_domain;


extern size_t (*_libc_terminal_read_func)(char *, size_t);
extern size_t (*_libc_terminal_write_func)(const char *, size_t);
extern void (*_libc_exit_func)(int);
extern void (*_libc_assert_func)(const char *, const char *, const char *, int);

void libc_exit(int);

__weak_reference(libc_exit, _exit);
void libc_exit(int status)
{   
    //TODO dereg from ns
    // debug_printf("Dereg pid %d, rpc: 0x%lx\n",disp_get_domain_id(),get_ns_rpc());
    errval_t err = aos_rpc_call(get_ns_rpc(),NS_DEREG_PROCESS,disp_get_domain_id());
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to remove self process from nameserver before exiting!\n");
    }
    debug_printf("libc exit NYI!\n");

    // // close stdout
    // err = aos_dc_close(&stdout_chan);
    // if(err_is_fail(err)){
    //     DEBUG_ERR(err,"Failed to close stdout\n");
    // }
    debug_printf("libc exit!\n");

    // // close stdout
    err = aos_dc_close(&stdout_chan);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to close stdout\n");
    }


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


size_t aos_terminal_write(const char *buf, size_t len)
{
    errval_t err;
    if (aos_dc_send_is_connected(&stdout_chan)) {
        err = aos_dc_send(&stdout_chan, len, buf);
        if (err_is_fail(err)) {
            return 0;
        }
        else {
            return len;
        }
    }
    return 0;
}


size_t aos_terminal_read(char *buf, size_t len)
{
    errval_t err = SYS_ERR_OK;
    size_t received;
    bool is_available = aos_dc_can_receive(&stdin_chan);

    void avail(void *arg) {
        is_available = true;
    }

    if (!is_available) {
        if (aos_dc_is_closed(&stdin_chan)) {
            return -1;
        }

        aos_dc_register(&stdin_chan, get_default_waitset(), MKCLOSURE(avail, NULL));
        while(!is_available) {
            err = event_dispatch(get_default_waitset());
            if (err_is_fail(err)) {
                aos_dc_deregister(&stdin_chan);
                debug_printf("Error in event_dispatch\n");
                return 0;
            }
            if (aos_dc_is_closed(&stdin_chan)) {
                aos_dc_deregister(&stdin_chan);
                return -1;
            }
        }
    }

    aos_dc_deregister(&stdin_chan);
    err = aos_dc_receive_available(&stdin_chan, len, buf, &received);
    return received;
}


/**
 * Set libc function pointers
 */
void barrelfish_libc_glue_init(void)
{
    // XXX: FIXME: Check whether we can use the proper kernel serial, and what we need for that
    // TODO: change these to use the user-space serial driver if possible
    // TODO: set these functions
    if (init_domain || 0) {
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

