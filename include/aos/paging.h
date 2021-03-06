/**
 * \file
 * \brief Barrelfish paging helpers.
 */

/*
 * Copyright (c) 2012, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Haldeneggsteig 4, CH-8092 Zurich. Attn: Systems Group.
 */


#ifndef LIBBARRELFISH_PAGING_H
#define LIBBARRELFISH_PAGING_H

#include <errors/errno.h>
#include <aos/capabilities.h>
#include <aos/slab.h>
#include <barrelfish_kpi/paging_arch.h>
#include <aos/except.h>
#include <aos/paging_types.h>

struct paging_state;
struct paging_region;


#define PAGING_LOCK(st) thread_mutex_lock_nested(&(st)->mutex)
#define PAGING_UNLOCK(st) thread_mutex_unlock(&(st)->mutex)


struct thread;
errval_t frame_alloc_and_map_flags(struct capref *cap,size_t bytes,size_t* retbytes,void **buf,int flags);

errval_t frame_alloc_and_map(struct capref *cap,size_t bytes,size_t* retbytes,void **buf);



errval_t paging_map_stack_guard(struct paging_state* ps, lvaddr_t stack_bottom);
void page_fault_handler(enum exception_type type, int subtype, void *addr, arch_registers_state_t *regs);
errval_t paging_map_single_page_at(struct paging_state *st, lvaddr_t addr, int flags, size_t pagesize);


errval_t paging_init_state(struct paging_state *st, lvaddr_t start_vaddr,
        struct capref pdir, struct slot_allocator * ca);
errval_t paging_init_state_foreign(struct paging_state *st, lvaddr_t start_vaddr,
        struct capref pdir, struct slot_allocator * ca);
/// initialize self-paging module
errval_t paging_init(void);

errval_t paging_init_stack(struct paging_state* ps);


void paging_init_onthread(struct thread *t);

errval_t paging_region_init(struct paging_state *st,
                            struct paging_region *pr, size_t size, paging_flags_t flags);
errval_t paging_region_init_fixed(struct paging_state *st, struct paging_region *pr,
                                 lvaddr_t base, size_t size, paging_flags_t flags);
errval_t paging_region_init_aligned(struct paging_state *st,
                                    struct paging_region *pr,
                                    size_t size, size_t alignment, paging_flags_t flags);

struct paging_region *paging_region_lookup(struct paging_state *st, lvaddr_t vaddr);


errval_t paging_region_delete(struct paging_state *ps, struct paging_region *pr);


/**
 * \brief return a pointer to a bit of the paging region `pr`.
 * This function gets used in some of the code that is responsible
 * for allocating Frame (and other) capabilities.
 */
errval_t paging_region_map(struct paging_region *pr, size_t req_size,
                           void **retbuf, size_t *ret_size);
/**
 * \brief free a bit of the paging region `pr`.
 * This function gets used in some of the code that is responsible
 * for allocating Frame (and other) capabilities.
 * We ignore unmap requests right now.
 */
errval_t paging_region_unmap(struct paging_region *pr, lvaddr_t base, size_t bytes);

/**
 * \brief Find a bit of free virtual address space that is large enough to
 *        accomodate a buffer of size `bytes`.
 */
errval_t paging_alloc(struct paging_state *st, void **buf, size_t bytes,
                      size_t alignment);





/**
 * Functions to map a user provided frame.
 */
/// Map user provided frame with given flags while allocating VA space for it
errval_t paging_map_frame_attr(struct paging_state *st, void **buf,
                               size_t bytes, struct capref frame,
                               int flags, void *arg1, void *arg2);

/// shadow page table lookup
errval_t paging_spt_find(struct paging_state *st, int level,
                         lvaddr_t addr, bool create,
                         struct mapping_table **ret);

/// Map user provided frame at user provided VA with given flags.
errval_t paging_map_fixed_attr(struct paging_state *st, lvaddr_t vaddr,
                               struct capref frame, size_t bytes, int flags);

/**
 * refill slab allocator without causing a page fault
 * 
 * \param frame references the capability slot where the frame cap can be put into
 */
errval_t slab_refill_no_pagefault(struct slab_allocator *slabs,
                                  struct capref frame, size_t minbytes);

/**
 * \brief unmap region starting at address `region`.
 * NOTE: this function is currently here to make libbarrelfish compile. As
 * noted on paging_region_unmap we ignore unmap requests right now.
 */
errval_t paging_unmap(struct paging_state *st, const void *region);


/// Map user provided frame while allocating VA space for it
static inline errval_t paging_map_frame(struct paging_state *st, void **buf,
                                        size_t bytes, struct capref frame,
                                        void *arg1, void *arg2)
{
    return paging_map_frame_attr(st, buf, bytes, frame,
            VREGION_FLAGS_READ_WRITE, arg1, arg2);
}

// Forward decl
static inline errval_t frame_identify(struct capref frame, struct frame_identity *ret);
/// Map complete user provided frame while allocating VA space for it
static inline errval_t paging_map_frame_complete(struct paging_state *st, void **buf,
                struct capref frame, void *arg1, void *arg2)
{
    errval_t err;
    struct frame_identity id;
    err = frame_identify(frame, &id);
    if(err_is_fail(err)){
        return err;
    }
    
    return paging_map_frame_attr(st, buf, id.bytes, frame,
            VREGION_FLAGS_READ_WRITE, arg1, arg2);
}

static inline errval_t paging_map_frame_complete_readable(struct paging_state *st, void **buf,
                struct capref frame, void *arg1, void *arg2)
{
    errval_t err;
    struct frame_identity id;
    err = frame_identify(frame, &id);
    if(err_is_fail(err)){
        return err;
    }
    return paging_map_frame_attr(st, buf, id.bytes, frame,
            VREGION_FLAGS_READ, arg1, arg2);
}

/// Map user provided frame at user provided VA.
static inline errval_t paging_map_fixed(struct paging_state *st, lvaddr_t vaddr,
                                        struct capref frame, size_t bytes)
{
    return paging_map_fixed_attr(st, vaddr, frame, bytes,
            VREGION_FLAGS_READ_WRITE);
}

static inline lvaddr_t paging_genvaddr_to_lvaddr(genvaddr_t genvaddr) {
    return (lvaddr_t) genvaddr;
}

#endif // LIBBARRELFISH_PAGING_H
