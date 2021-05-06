
/**
 * \file
 * \brief Manage domain spanning cores
 *
 * \bug Need to specify how big the default thread on the spanned dispatcher
 * should be because we cannot dynamically grow our stacks
 *
 * \bug Can only do domain_new_dispatcher() when no other dispatchers have
 * threads (except for the internal interdisp-thread).
 */

/*
 * Copyright (c) 2009, 2010, 2011, 2012, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, CAB F.78, Universitaetstr. 6, CH-8092 Zurich,
 * Attn: Systems Group.
 */

#include <limits.h>
#include <stdio.h>
#include <aos/aos.h>
#include <aos/curdispatcher_arch.h>
#include <aos/dispatcher_arch.h>
#include <aos/waitset_chan.h>
#include <barrelfish_kpi/domain_params.h>
#include <arch/registers.h>
#include <aos/dispatch.h>
#include "arch/threads.h"
#include "init.h"
#include "threads_priv.h"
#include "waitset_chan_priv.h"

/**
 * \brief set the core_id.
 *
 * Code using this should do a kernel_cap invocation to get the core_id first.
 */
void disp_set_core_id(coreid_t core_id)
{
    dispatcher_handle_t handle = curdispatcher();
    struct dispatcher_generic* disp = get_dispatcher_generic(handle);
    disp->core_id = core_id;
}


/**
 * \brief returns the address and the size of the EH frame
 *
 * \param eh_frame      returned virtual address of the EH frame
 * \param eh_frame_size returned size of the EH frame
 */
void disp_get_eh_frame(lvaddr_t *eh_frame,
                       size_t *eh_frame_size)
{
    dispatcher_handle_t handle = curdispatcher();
    struct dispatcher_generic* disp = get_dispatcher_generic(handle);
    if (eh_frame) {
        *eh_frame = disp->eh_frame;
    }
    if (eh_frame_size) {
        *eh_frame_size = disp->eh_frame_size;
    }
}

/**
 * \brief returns the address and the size of the EH frame header
 *
 * \param eh_frame      returned virtual address of the EH frame
 * \param eh_frame_size returned size of the EH frame
 */
void disp_get_eh_frame_hdr(lvaddr_t *eh_frame_hdr,
                       size_t *eh_frame_hdr_size)
{
    dispatcher_handle_t handle = curdispatcher();
    struct dispatcher_generic* disp = get_dispatcher_generic(handle);
    if (eh_frame_hdr) {
        *eh_frame_hdr = disp->eh_frame_hdr;
    }
    if (eh_frame_hdr_size) {
        *eh_frame_hdr_size = disp->eh_frame_hdr_size;
    }
}

/**
 * \brief returns the core_id stored in disp_priv struct
 */
coreid_t disp_get_core_id(void)
{
    dispatcher_handle_t handle = curdispatcher();
    struct dispatcher_generic* disp = get_dispatcher_generic(handle);
    return disp->core_id;
}

/**
 * \brief returns the current core_id stored in disp_shared struct
 */
coreid_t disp_get_current_core_id(void)
{
    dispatcher_handle_t handle = curdispatcher();
    struct dispatcher_shared_generic* disp = get_dispatcher_shared_generic(handle);
    return disp->curr_core_id;
}

/**
 * \brief returns the domain_id stored in disp_priv struct
 */
domainid_t disp_get_domain_id(void)
{
    dispatcher_handle_t handle = curdispatcher();
    struct dispatcher_generic* disp = get_dispatcher_generic(handle);
    return disp->domain_id;
}



/**
 * \brief Sets domain id for the dispatcher, NOTE: do not do this if you are not init process in init_foreign core(ensures uniqueness of pids)
 */
void disp_set_domain_id(domainid_t pid)
{
    dispatcher_handle_t handle = curdispatcher();
    struct dispatcher_generic* disp = get_dispatcher_generic(handle);
     disp->domain_id = pid;
}


/**
 * \brief returns the core_id stored in disp_priv struct
 */
coreid_t disp_handle_get_core_id(dispatcher_handle_t handle)
{
    struct dispatcher_generic* disp = get_dispatcher_generic(handle);
    return disp->core_id;
}

struct waitset *get_default_waitset(void)
{
    dispatcher_handle_t handle = curdispatcher();
    struct dispatcher_generic* disp = get_dispatcher_generic(handle);
    return &disp->core_state.c.default_waitset;
}

/**
 * \brief Returns a pointer to the morecore state on the dispatcher priv
 */
struct morecore_state *get_morecore_state(void)
{
    dispatcher_handle_t handle = curdispatcher();
    struct dispatcher_generic* disp = get_dispatcher_generic(handle);
    return &disp->core_state.c.morecore_state;
}

/**
 * \brief Returns a pointer to the paging state on the dispatcher priv
 */
struct paging_state *get_current_paging_state(void)
{
    dispatcher_handle_t handle = curdispatcher();
    struct dispatcher_generic *disp = get_dispatcher_generic(handle);
    return disp->core_state.c.paging_state;
}

void set_current_paging_state(struct paging_state *st)
{
    dispatcher_handle_t handle = curdispatcher();
    struct dispatcher_generic *disp = get_dispatcher_generic(handle);
    disp->core_state.c.paging_state = st;
}

/**
 * \brief Returns a pointer to the ram_alloc state on the dispatcher priv
 */
struct ram_alloc_state *get_ram_alloc_state(void)
{
    dispatcher_handle_t handle = curdispatcher();
    struct dispatcher_generic* disp = get_dispatcher_generic(handle);
    return &disp->core_state.c.ram_alloc_state;
}

/**
 * \brief Returns a pointer to the spawn state on the dispatcher priv
 */
struct slot_alloc_state *get_slot_alloc_state(void)
{
    dispatcher_handle_t handle = curdispatcher();
    struct dispatcher_generic* disp = get_dispatcher_generic(handle);
    return &disp->core_state.c.slot_alloc_state;
}

/**
 * \brief set the init client binding on the dispatcher priv
 */
void set_init_chan(struct aos_chan *initchan)
{
    dispatcher_handle_t handle = curdispatcher();
    struct dispatcher_generic* disp = get_dispatcher_generic(handle);
    disp->core_state.c.init_chan = initchan;
}

/**
 * \brief Returns the monitor client binding on the dispatcher priv
 */
struct aos_chan *get_init_chan(void)
{
    dispatcher_handle_t handle = curdispatcher();
    struct dispatcher_generic* disp = get_dispatcher_generic(handle);
    return disp->core_state.c.init_chan;
}

/**
 * \brief Set the init rpc channel on the domain state
 */
void set_init_rpc(struct aos_rpc *initrpc)
{
    dispatcher_handle_t handle = curdispatcher();
    struct dispatcher_generic* disp = get_dispatcher_generic(handle);
    disp->core_state.c.init_rpc = initrpc;
}

/**
 * \brief Returns the RPC channel to init 
 */
struct aos_rpc *get_init_rpc(void)
{
    dispatcher_handle_t handle = curdispatcher();
    struct dispatcher_generic* disp = get_dispatcher_generic(handle);
    return disp->core_state.c.init_rpc;
}


void set_pm_online(void){
    dispatcher_handle_t handle = curdispatcher();
    struct dispatcher_generic* disp = get_dispatcher_generic(handle);
    disp->core_state.c.pm_online = true;
} 


bool get_pm_online(void){
    dispatcher_handle_t handle = curdispatcher();
    struct dispatcher_generic* disp = get_dispatcher_generic(handle);
    return disp->core_state.c.pm_online;
} 

void set_pm_rpc(struct aos_rpc *pm_rpc){
    dispatcher_handle_t handle = curdispatcher();
    struct dispatcher_generic* disp = get_dispatcher_generic(handle);
    disp->core_state.c.pm_rpc = pm_rpc;
}

struct aos_rpc* get_pm_rpc(void){
    dispatcher_handle_t handle = curdispatcher();
    struct dispatcher_generic* disp = get_dispatcher_generic(handle);
    return disp->core_state.c.pm_rpc;
}


void set_mem_online(void){
    dispatcher_handle_t handle = curdispatcher();
    struct dispatcher_generic* disp = get_dispatcher_generic(handle);
    disp->core_state.c.mem_online = true;
}

bool get_mem_online(void){
    dispatcher_handle_t handle = curdispatcher();
    struct dispatcher_generic* disp = get_dispatcher_generic(handle);
    return disp->core_state.c.mem_online;
} 


void set_mem_rpc(struct aos_rpc *mem_rpc){
    dispatcher_handle_t handle = curdispatcher();
    struct dispatcher_generic* disp = get_dispatcher_generic(handle);
    disp->core_state.c.mem_rpc = mem_rpc;
}

struct aos_rpc* get_mem_rpc(void){
    dispatcher_handle_t handle = curdispatcher();
    struct dispatcher_generic* disp = get_dispatcher_generic(handle);
    return disp->core_state.c.mem_rpc;
}


void set_init_domain(void){
    dispatcher_handle_t handle = curdispatcher();
    struct dispatcher_generic* disp = get_dispatcher_generic(handle);
    disp -> core_state.c.init_domain = true;
}
bool get_init_domain(void){
    dispatcher_handle_t handle = curdispatcher();
    struct dispatcher_generic* disp = get_dispatcher_generic(handle);
    return disp -> core_state.c.init_domain;
}


struct aos_rpc* get_core_channel(coreid_t core_id){
    assert(core_id < 4 && "Tried to get channel for core >= 4!");
    dispatcher_handle_t handle = curdispatcher();
    struct dispatcher_generic* disp = get_dispatcher_generic(handle);
    struct aos_rpc *ret = disp->core_state.c.core_channels[core_id];
    if (ret == NULL) {
        debug_printf("Returning NULL channel\n");
    }
    return ret;
}

void set_core_channel(coreid_t core_id, struct aos_rpc * core_channel){
    assert(core_id < 4 && "Tried to set channel for core >= 4!");
    dispatcher_handle_t handle = curdispatcher();
    struct dispatcher_generic* disp = get_dispatcher_generic(handle);
    disp -> core_state.c.core_channels[core_id] = core_channel;
}