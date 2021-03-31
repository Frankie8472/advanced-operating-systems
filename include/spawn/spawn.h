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



struct spawninfo {
    // the next in the list of spawned domains
    struct spawninfo *next;

    // Information about the binary
    char *binary_name;     // Name of the binary

    lvaddr_t mapped_elf;
    size_t mapped_elf_size;

    struct paging_state ps;
    struct capref dispatcher;
    struct capref cap_ep;
    struct lmp_endpoint *lmp_ep;

    // TODO(M2): Add fields you need to store state
    //           when spawning a new dispatcher,
    //           e.g. references to the child's
    //           capabilities or paging state

};

// Start a child process using the multiboot command line. Fills in si.
errval_t spawn_load_by_name(char *binary_name, struct spawninfo *si,
                            domainid_t *pid);

// setup cspace for a dispatcher
errval_t setup_c_space(struct capref, struct cnoderef *, struct cnoderef *, struct cnoderef *, struct cnoderef *, struct cnoderef *, struct cnoderef *);

// Start a child with an explicit command line. Fills in si.
errval_t spawn_load_argv(int argc, const char *const argv[], struct spawninfo *si,
                         domainid_t *pid);



/// elf callback function
errval_t allocate_elf_memory(void* state, genvaddr_t base, size_t size, uint32_t flags, void **ret);

#endif /* _INIT_SPAWN_H_ */
