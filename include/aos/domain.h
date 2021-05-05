/**
 * \file
 * \brief
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

#ifndef BARRELFISH_DOMAIN_H
#define BARRELFISH_DOMAIN_H

#include <sys/cdefs.h>
#include <aos/event_queue.h>
#include <aos/threads.h>

__BEGIN_DECLS

typedef void (*domain_spanned_callback_t)(void *arg, errval_t err);

struct aos_chan;
struct aos_rpc;
struct waitset;


struct waitset *get_default_waitset(void);
void disp_set_core_id(coreid_t core_id);
coreid_t disp_get_core_id(void);
coreid_t disp_get_current_core_id(void);
void disp_get_eh_frame(lvaddr_t *eh_frame, size_t *eh_frame_size);
void disp_get_eh_frame_hdr(lvaddr_t *eh_frame_hdr, size_t *eh_frame_hdr_size);
domainid_t disp_get_domain_id(void);
void disp_set_domain_id(domainid_t pid);
coreid_t disp_handle_get_core_id(dispatcher_handle_t handle);
void set_init_chan(struct aos_chan *initchan);
struct aos_chan *get_init_chan(void);
void set_init_rpc(struct aos_rpc *initrpc);
struct aos_rpc *get_init_rpc(void);
struct morecore_state *get_morecore_state(void);
struct paging_state *get_current_paging_state(void);
void set_current_paging_state(struct paging_state *st);
struct ram_alloc_state *get_ram_alloc_state(void);
struct slot_alloc_state *get_slot_alloc_state(void);

void set_pm_online(void);
void set_mem_online(void);
bool get_pm_online(void);
bool get_mem_online(void);

void set_mem_rpc(struct aos_rpc *mem_rpc);
struct aos_rpc* get_mem_rpc(void);

void set_pm_rpc(struct aos_rpc *pm_rpc);
struct aos_rpc* get_pm_rpc(void);


void set_init_domain(void);
bool get_init_domain(void);

struct aos_rpc* get_core_channel(coreid_t core_id);

void set_core_channel(coreid_t core_id, struct aos_rpc *);


__END_DECLS

#endif
