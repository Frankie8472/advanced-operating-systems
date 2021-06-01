/**
 * \file
 * \brief Barrelfish library initialization.
 */

/*
 * Copyright (c) 2007, 2008, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Haldeneggsteig 4, CH-8092 Zurich. Attn: Systems Group.
 */

#ifndef LIBBARRELFISH_INIT_H
#define LIBBARRELFISH_INIT_H

#include <aos/aos_rpc.h>

struct spawn_domain_params;
errval_t barrelfish_init_onthread(struct spawn_domain_params *params);
void barrelfish_libc_glue_init(void);
//


// void handle_all_binding_request_on_process(struct aos_rpc *r, uintptr_t pid, uintptr_t core_id,uintptr_t client_core ,struct capref client_cap,struct capref * server_cap);


void handle_send_number(struct aos_rpc *r, uintptr_t number);
void handle_send_string(struct aos_rpc *r, const char *string);
void handle_send_varbytes(struct aos_rpc *r, struct aos_rpc_varbytes bytes);
void initialize_general_purpose_handler(struct aos_rpc* rpc);
#endif
