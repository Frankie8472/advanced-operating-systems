/**
 * \file
 * \brief create child process library
 */

/*
 * Copyright (c) 2016, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetsstrasse 6, CH-8092 Zurich. Attn: Systems Group.
 */

#ifndef _INIT_SPAWN_H_
#define _INIT_SPAWN_H_

#include "aos/slot_alloc.h"
#include "aos/paging.h"
#include "aos/aos_rpc.h"



struct spawninfo {
    // the next in the list of spawned domains
    struct spawninfo *next;

    // Information about the binary
    char *binary_name;     // Name of the binary
    int argc;
    char **argv;
    int envc;
    char **envp;

    // root cnode of the child
    struct capref rootcn;

    struct capref dispatcher_cap;
    struct capref dispframe_cap;
    // cap to put into the new domain's cspace in the
    // slot for the spawner_ep
    struct lmp_endpoint *spawner_ep;
    struct capref spawner_ep_cap;

    lvaddr_t mapped_elf;
    size_t mapped_elf_size;

    bool spawned;
    domainid_t pid;

    struct paging_state ps;
    struct capref dispatcher;
    struct capref cap_ep, child_ep;
    struct lmp_endpoint *lmp_ep;
    struct aos_rpc rpc;


    struct aos_rpc disp_rpc;

    struct capref child_stdout_cap;
    struct capref child_stdin_cap;
    struct lmp_endpoint *child_stdout;

    // TODO(M2): Add fields you need to store state
    //           when spawning a new dispatcher,
    //           e.g. references to the child's
    //           capabilities or paging state

};

errval_t spawn_load_by_name_setup(char *binary_name, struct spawninfo *si);

// Start a child process using the multiboot command line. Fills in si.
errval_t spawn_load_by_name(char *binary_name, struct spawninfo *si,
                            domainid_t *pid);

// Do the same as spawn_load_by_name but don't start the dispatcher yet
errval_t spawn_setup_by_name(char *binary_name, struct spawninfo *si,
                            domainid_t *pid);

// only map module
errval_t spawn_setup_module_by_name(const char *binary_name, struct spawninfo *si);

// setup cspace for a dispatcher
errval_t setup_c_space(struct capref, struct cnoderef *, struct cnoderef *, struct cnoderef *, struct cnoderef *, struct cnoderef *, struct cnoderef *);

errval_t spawn_setup_dispatcher(int argc, const char *const argv[], struct spawninfo *si,
                domainid_t *pid);
errval_t spawn_invoke_dispatcher(struct spawninfo *si);

// Start a child with an explicit command line. Fills in si.
errval_t spawn_load_argv(int argc, const char *const argv[], struct spawninfo *si,
                         domainid_t *pid);



/// elf callback function
errval_t allocate_elf_memory(void* state, genvaddr_t base, size_t size, uint32_t flags, void **ret);


#endif /* _INIT_SPAWN_H_ */
