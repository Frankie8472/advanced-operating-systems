/**
 * \file
 * \brief AOS paging helpers.
 */

/*
 * Copyright (c) 2012, 2013, 2016, ETH Zurich.W
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetstr. 6, CH-8092 Zurich. Attn: Systems Group.
 */

#include <aos/aos.h>
#include <aos/paging.h>
#include <aos/except.h>
#include <aos/slab.h>
#include <aos/systime.h>
#include "threads_priv.h"
#include <stdio.h>
#include <string.h>

static struct paging_state current;


void page_fault_handler(enum exception_type type, int subtype, void *addr, arch_registers_state_t *regs) {
    errval_t err;

    //debug_printf("handling pagefault!\n");
    //debug_printf("type: %d\n", type);
    //debug_printf("subtype: %d\n", subtype);
    //debug_printf("addr: 0x%" PRIxLPADDR "\n", addr);
    //debug_printf("ip: 0x%" PRIxLPADDR "\n", regs->named.pc);

    struct paging_state *st = get_current_paging_state();

    if (type == EXCEPT_PAGEFAULT) {
        // look up, in which region the page fault happened
        struct paging_region *region = paging_region_lookup(st, (lvaddr_t) addr);
        if (region == NULL) {
            debug_printf("error in page handler: can't find paging region\n");
            debug_printf("this is probably due to a faulty region list\n");
            thread_exit(1);
        }

        if (region->type == PAGING_REGION_STACK && ((lvaddr_t) addr) < region->base_addr + BASE_PAGE_SIZE) {
            // if it is in the first page of a stack region, we consider it a stack overflow
            debug_printf("Stack overflow!\n\n");
            debug_printf("addr: 0x%" PRIxLPADDR "\n", addr);
            debug_printf("ip: 0x%" PRIxLPADDR "\n", regs->named.pc);
            long from_region_end = region->base_addr + region->region_size - regs->named.stack;
            debug_printf("stack pointer: 0x%" PRIxLPADDR " (%ld bytes from region end)\n", regs->named.stack, from_region_end);
            thread_exit(1);
        }
        else if (region->lazily_mapped) {
            // in a lazily mapped region we should only page fault if a page is not mapped, so we map it
            err = paging_map_single_page_at(st,
                    (lvaddr_t) addr,
                    VREGION_FLAGS_READ_WRITE,
                    region->map_large_pages ? LARGE_PAGE_SIZE : BASE_PAGE_SIZE
            );
            
            if (err_is_fail(err)) {
                DEBUG_ERR(err, "error mapping page in page fauilt handler\n");
                thread_exit(1);
            }
            return;
        }
        else {
            // non-lazily-mapped regions should not page fault --> display error
            debug_printf("pagefault occurred in non-lazily mapped region\n");
            debug_printf("region: %lx, %lx\n", region->base_addr, region->region_size);

            region->region_name[sizeof region->region_name - 1] = 0; // assert null terminator
            debug_printf("region name: %s\n", region->region_name);
            debug_printf("addr: 0x%" PRIxLPADDR "\n", addr);
            debug_printf("ip: 0x%" PRIxLPADDR "\n", regs->named.pc);
            debug_printf("dumping stack\n");

            // this might page fault again, but no biggie, we are terminating anyways
            debug_dump_mem_around_addr(regs->named.stack);
            thread_exit(1);
        }
    };
    thread_exit(0);
}


/**
 * \brief creates a frame and maps it around the specified address
 * 
 * This function is called from the page fault handler if a lazily mapped page
 * should be allocated.
 */
errval_t paging_map_single_page_at(struct paging_state *st, lvaddr_t addr, int flags, size_t pagesize)
{
    assert(st != 0);
    assert(pagesize == BASE_PAGE_SIZE || pagesize == LARGE_PAGE_SIZE);

    struct capref frame;
    size_t retbytes;
    errval_t err = frame_alloc_aligned(&frame, pagesize, pagesize, &retbytes);
    ON_ERR_RETURN(err);

    lvaddr_t vaddr = ROUND_DOWN((lvaddr_t) addr, pagesize);

    err = paging_map_fixed_attr(st, vaddr, frame, pagesize, flags);
    ON_ERR_RETURN(err);

    return SYS_ERR_OK;
}


/**
 * TODO(M2): Implement this function.
 * TODO(M4): Improve this function.
 * \brief Initialize the paging_state struct for the paging
 *        state of the calling process. (Shadowpagetable)
 * 
 * \param st The struct to be initialized, must not be NULL.
 * \param start_vaddr Virtual address allocation should start at
 *        this address.
 * \param pdir Reference to the cap of the L0 VNode.
 * \param ca The slot_allocator to be used by the paging state.
 * \return Either SYS_ERR_OK if no error occured or an error
 * indicating what went wrong otherwise.
 */
errval_t paging_init_state(struct paging_state *st, lvaddr_t start_vaddr,
                           struct capref pdir, struct slot_allocator *ca)
{
    // TODO (M2): Implement state struct initialization
    // TODO (M4): Implement page fault handler that installs frames when a page fault
    assert(st != NULL);

    // Init paging state struct
    memset(st, 0, sizeof(struct paging_state));
    st->slot_alloc = ca;

    // Initialize shadowpagetable
    st->mappings_alloc_is_refilling = false;
    st->map_l0.pt_cap = pdir;

    // Init allocator for shadowpagetable
    static char init_mem[SLAB_STATIC_SIZE(16, sizeof(struct mapping_table))];
    slab_init(&st->mappings_alloc, sizeof(struct mapping_table), NULL);
    slab_grow(&st->mappings_alloc, init_mem, sizeof(init_mem));

    struct paging_region *free_region = &st->free_region;
    free_region->base_addr = 0;
    free_region->region_size = 0x0000FFFFFFFFFFFFULL;
    free_region->type = PAGING_REGION_FREE;
    free_region->prev = NULL;
    free_region->next = NULL;
    st->head = &st->free_region;

    paging_region_init(st, &st->vaddr_offset_region, start_vaddr, 0);
    st->vaddr_offset_region.lazily_mapped = false;
    st->vaddr_offset_region.type = PAGING_REGION_UNUSABLE;
    strncpy(st->vaddr_offset_region.region_name, "vaddr offset", sizeof(st->vaddr_offset_region.region_name));

    paging_region_init(st, &st->meta_region, 1L << 43, VREGION_FLAGS_READ_WRITE);
    st->meta_region.lazily_mapped = false;
    st->meta_region.type = PAGING_REGION_UNUSABLE;
    strncpy(st->meta_region.region_name, "meta region", sizeof st->meta_region.region_name);

    paging_region_init(st, &st->heap_region, 1L << 42, VREGION_FLAGS_READ_WRITE);
    st->heap_region.type = PAGING_REGION_HEAP;
    st->heap_region.map_large_pages = true;
    strncpy(st->heap_region.region_name, "heap region", sizeof(st->heap_region.region_name));


    return SYS_ERR_OK;
}

/**
 * TODO(M2): Implement this function.
 * TODO(M4): Improve this function.
 * \brief Initialize the paging_state struct for the paging state
 *        of a child process.
 *        Only so far, that we are able fill out the needed data for the process to start!
 * 
 * \param st The struct to be initialized, must not be NULL.
 * \param start_vaddr Virtual address allocation should start at
 *        this address.
 * \param pdir Reference to the cap of the L0 VNode.
 * \param ca The slot_allocator to be used by the paging state.
 * \return Either SYS_ERR_OK if no error occured or an error
 * indicating what went wrong otherwise.
 */
errval_t paging_init_state_foreign(struct paging_state *st, lvaddr_t start_vaddr,
                           struct capref pdir, struct slot_allocator *ca)
{
    // TODO (M2): Implement state struct initialization
    // TODO (M4): Implement page fault handler that installs frames when a page fault
    assert(st != NULL);
    errval_t err;

    // Init paging state struct
    memset(st, 0, sizeof(struct paging_state));
    st->slot_alloc = ca;

    // Initialize shadowpagetable
    st->map_l0.pt_cap = pdir;

    // Init allocator for shadowpagetable
    struct capref frame;
    size_t actual_size;
    err = frame_alloc(&frame, ROUND_UP(sizeof(struct mapping_table) * 32, BASE_PAGE_SIZE), &actual_size);
    ON_ERR_PUSH_RETURN(err, LIB_ERR_FRAME_ALLOC);

    void *slab_memory;
    err = paging_map_frame_complete(get_current_paging_state(), &slab_memory, frame, NULL, NULL);
    ON_ERR_PUSH_RETURN(err, LIB_ERR_PMAP_MAP);
    slab_init(&st->mappings_alloc, sizeof(struct mapping_table), NULL);
    slab_grow(&st->mappings_alloc, slab_memory, actual_size);

    // Initialize vaddr region linked list will be done by the process itself
    // Reserving VSPACE for the meta- (8TB), heap- (4TB) and stackregion (1GB) will be done by the process itself
    return SYS_ERR_OK;
}


errval_t paging_map_stack_guard(struct paging_state* ps, lvaddr_t stack_bottom) {
    struct capref frame;
    errval_t err;
    err = frame_alloc(&frame, BASE_PAGE_SIZE, NULL);
    ON_ERR_RETURN(err);

    err = paging_map_fixed_attr(ps, ROUND_DOWN(stack_bottom, BASE_PAGE_SIZE), frame, BASE_PAGE_SIZE, 0);
    ON_ERR_RETURN(err);

    return SYS_ERR_OK;
}


/**
 * \brief This function initializes the paging for this domain
 * It is called once in the init process before main runs.
 */
errval_t paging_init(void)
{
    // TODO (M2): Call paging_init_state for &current
    // TODO (M4): initialize self-paging handler
    // TIP: use thread_set_exception_handler() to setup a page fault handler
    // TIP: Think about the fact that later on, you'll have to make sure that
    // you can handle page faults in any thread of a domain.
    // TIP: it might be a good idea to call paging_init_state() from here to
    // avoid code duplication.
    //debug_printf("paging_init\n");

    struct capref root_pagetable = {
        .cnode = cnode_page,
        .slot  = 0
    };

    errval_t err;
    static char new_stack[32 * 1024];
    // void* new_stack = malloc(BASE_PAGE_SIZE);

    exception_handler_fn handler = (exception_handler_fn) page_fault_handler;
    err = thread_set_exception_handler(handler, NULL, new_stack, new_stack + sizeof(new_stack), NULL, NULL);
    ON_ERR_RETURN(err);

    err = paging_init_state(&current, VADDR_OFFSET, root_pagetable, get_default_slot_allocator());
    ON_ERR_PUSH_RETURN(err, LIB_ERR_VSPACE_INIT);

    set_current_paging_state(&current);
    //err = slot_alloc_init();
    ON_ERR_PUSH_RETURN(err, LIB_ERR_SLOT_ALLOC_INIT);

    return SYS_ERR_OK;
}


/**
 * \brief Initialize per-thread paging state
 */
void paging_init_onthread(struct thread *t)
{
    // TODO (M4): setup exception handler for thread `t'.
    assert(t != NULL);
    
    size_t exception_stack_size = 8 * 1024; // 8 KB for exception handler
    char *exception_stack = malloc(exception_stack_size);

    // as malloc'ed memory might not be paged yet, we write to it
    // (we should not have page faults inside the page fault handler)
    for (size_t i = 0; i < exception_stack_size; i += BASE_PAGE_SIZE) {
        exception_stack[i] = 0;
    }
    t->exception_handler = page_fault_handler;
    t->exception_stack = exception_stack;
    t->exception_stack_top = (void *) ROUND_DOWN((lvaddr_t) exception_stack + exception_stack_size, 32);
}   

/**
 * \brief Initialize a paging region in `pr`, such that it  starts
 * from base and contains size bytes.
 */
errval_t paging_region_init_fixed(struct paging_state *st, struct paging_region *pr,
                                  lvaddr_t base, size_t size, paging_flags_t flags)
{
    assert(st != NULL && pr != NULL);
    
    // currently should not be used
    return LIB_ERR_NOT_IMPLEMENTED;

    pr->base_addr = (lvaddr_t) base;
    pr->current_addr = pr->base_addr;
    pr->region_size = size;
    pr->flags = flags;

    //TODO(M2): Add the region to a datastructure and ensure paging_alloc
    //will return non-overlapping regions.
    // TODO inspect this; maybe replace with smarter allocating algorithm
    return SYS_ERR_OK;
}

/**
 * \brief Initialize a paging region in `pr`, such that it contains at least
 * size bytes and is aligned to a multiple of alignment.
 */
errval_t paging_region_init_aligned(struct paging_state *st, struct paging_region *pr,
                                    size_t size, size_t alignment, paging_flags_t flags)
{
    assert(st != NULL && pr != NULL);

    if (alignment > LARGE_PAGE_SIZE) {
        // currently not supported
        return LIB_ERR_PMAP_INIT;
    }
    else {
        return paging_region_init(st, pr, size, flags);
    }
}


struct paging_region *paging_region_lookup(struct paging_state *st, lvaddr_t vaddr)
{
    // naive implementation; TODO improve
    
    const uint64_t pt_index[4] = {
        (vaddr >> (12 + 3 * 9)) & 0x1FF,
        (vaddr >> (12 + 2 * 9)) & 0x1FF,
        (vaddr >> (12 + 1 * 9)) & 0x1FF,
        (vaddr >> (12 + 0 * 9)) & 0x1FF,
    };

    struct mapping_table *nearest = &st->map_l0;
    for (size_t i = 0; i < 3; i++) {
        struct mapping_table *nearest_child = nearest->children[pt_index[i]];
        if (nearest_child != NULL) {
            nearest = nearest_child;
        }
        else {
            break;
        }
    }

    struct paging_region *region = nearest->region ? : st->head;
    for (; region != NULL; region = region->next) {
        if (region->base_addr <= vaddr && region->base_addr + region->region_size > vaddr) {
            return region;
        }
    }
    return NULL;
}


static bool pr_intersects(lvaddr_t addr, size_t size, struct paging_region *pr) {
    return (addr + size > pr->base_addr) || (pr->base_addr + pr->region_size > addr);
}

static bool pr_inside(lvaddr_t addr, struct paging_region *pr) {
    return (addr >= pr->base_addr) && (addr < pr->base_addr + pr->region_size);
}

static errval_t update_spt(struct mapping_table *mt, lvaddr_t mt_start, int level, struct paging_region *pr)
{
    if (pr_inside(mt_start, pr)) {
        mt->region = pr;
    }

    int shift_level = 12 + (3 - level) * 9;
    for (size_t i = 0; i < PTABLE_ENTRIES; i++) {
        lvaddr_t this_start = mt_start + (i << shift_level);

        if (level < 3 && mt->children[i] != NULL && 
                pr_intersects(this_start, 1 << shift_level, pr)) {
            update_spt(mt->children[i], this_start, level + 1, pr);
        }
    }
    return SYS_ERR_OK;
}


static errval_t update_region_lookups(struct paging_state *st, struct paging_region *pr)
{
    return update_spt(&st->map_l0, 0, 0, pr);
}


/**
 * \brief Initialize a paging region in `pr`, such that it contains at least
 * size bytes.
 *
 * This function gets used in some of the code that is responsible
 * for allocating Frame (and other) capabilities.
 * 
 * \note currently all paging regions are 2 MiB aligned
 */
errval_t paging_region_init(struct paging_state *st, struct paging_region *pr,
                            size_t size, paging_flags_t flags)
{
    assert(st != NULL && pr != NULL);

    // make sure all paging regions are 2 MiB aligned
    size = ROUND_UP(size, LARGE_PAGE_SIZE);

    struct paging_region *region = st->head;
    for (; region != NULL; region = region->next) {
        if (region->type == PAGING_REGION_FREE && region->region_size >= size) {
            break;
        }
    }

    NULLPTR_CHECK(region, LIB_ERR_VSPACE_MMU_AWARE_NO_SPACE);

    // set lazy mapping to true as default
    pr->lazily_mapped = true;

    pr->region_size = size;
    pr->flags = flags;
    pr->next = region;
    pr->prev = region->prev;
    pr->base_addr = region->base_addr;
    pr->current_addr = pr->base_addr;

    if (st->head == region) {
        st->head = pr;
    }

    region->base_addr += size;
    region->region_size -= size;
    region->current_addr = region->base_addr;
    region->prev = pr;

    if(pr->prev != NULL) {
        pr->prev->next = pr;
    }
    
    errval_t err = update_region_lookups(st, pr);
    ON_ERR_RETURN(err);

    return SYS_ERR_OK;
    //return paging_region_init_aligned(st, pr, size, BASE_PAGE_SIZE, flags);
}

errval_t paging_region_delete(struct paging_state *ps, struct paging_region *pr)
{
    // TODO: implement
    return LIB_ERR_NOT_IMPLEMENTED;
}

/**
 * \brief return a pointer to a bit of the paging region `pr`.
 * This function gets used in some of the code that is responsible
 * for allocating Frame (and other) capabilities.
 */
errval_t paging_region_map(struct paging_region *pr, size_t req_size, void **retbuf,
                           size_t *ret_size)
{
    assert(pr != NULL);

    lvaddr_t end_addr = pr->base_addr + pr->region_size;
    ssize_t rem = end_addr - pr->current_addr;
    if (rem >= req_size) {
        // ok
        *retbuf = (void *)pr->current_addr;
        *ret_size = req_size;
        pr->current_addr += req_size;
    } else if (rem > 0) {
        *retbuf = (void *)pr->current_addr;
        *ret_size = rem;
        pr->current_addr += rem;
        debug_printf("exhausted paging region, "
                     "expect badness on next allocation\n");
    } else {
        return LIB_ERR_VSPACE_MMU_AWARE_NO_SPACE;
    }
    return SYS_ERR_OK;
}

/**
 * TODO(M2): As an OPTIONAL part of M2 implement this function
 * \brief free a bit of the paging region `pr`.
 * This function gets used in some of the code that is responsible
 * for allocating Frame (and other) capabilities.
 * NOTE: Implementing this function is optional.
 */
errval_t paging_region_unmap(struct paging_region *pr, lvaddr_t base, size_t bytes)
{
    // XXX: should free up some space in paging region, however need to track
    //      holes for non-trivial case
    assert(pr != NULL);

    return LIB_ERR_NOT_IMPLEMENTED;
}

/** 
 * TODO(M2): Implement this function.
 * \brief Find a bit of free virtual address space that is large enough to accomodate a
 *        buffer of size 'bytes'.
 * 
 * \param st A pointer to the paging state.
 * \param buf This parameter is used to return the free virtual address that was found.
 * \param bytes The number of bytes that need to be free (at the minimum) at the found
 *        virtual address.
 * \param alignment The address needs to be a multiple of 'alignment'.
 * \return Either SYS_ERR_OK if no error occured or an error
 *        indicating what went wrong otherwise.
 */

errval_t paging_alloc(struct paging_state *st, void **buf, size_t bytes, size_t alignment)
{
    /**
     * TODO(M2): Implement this function
     * \brief Find a bit of free virtual address space that is large enough to
     *        accomodate a buffer of size `bytes`.
     */
    assert(st != NULL);
    /*
    if (st->current_address + bytes < st->current_address) {
        HERE;
        DEBUG_PRINTF("critical: vspace exhausted");
        return LIB_ERR_VSPACE_MMU_AWARE_NO_SPACE;
    }

    lvaddr_t base = ROUND_UP(st->current_address, alignment);
    st->current_address = ROUND_UP(base + ROUND_UP(bytes, BASE_PAGE_SIZE), BASE_PAGE_SIZE);

    *buf = (void*) base;
    */
    return SYS_ERR_OK;
}

/**
 * TODO(M2): Implement this function.
 * \brief Finds a free virtual address and maps a frame at that address
 * 
 * \param st A pointer to the paging state.
 * \param buf This will parameter will be used to return the free virtual
 * address at which a new frame as been mapped.
 * \param bytes The number of bytes that need to be free (at the minimum)
 *        at the virtual address found.
 * \param frame A reference to the frame cap that is supposed to be mapped.
 * \param flags The flags that are to be set for the newly mapped region,
 *        see 'paging_flags_t' in paging_types.h .
 * \param arg1 Currently unused argument.
 * \param arg2 Currently unused argument.
 * \return Either SYS_ERR_OK if no error occured or an error
 * indicating what went wrong otherwise.
 */



errval_t paging_map_frame_attr(struct paging_state *st, void **buf, size_t bytes,
                               struct capref frame, int flags, void *arg1, void *arg2)
{
    // TODO(M2): Implement me
    // - Call paging_alloc to get a free virtual address region of the requested size
    // - Map the user provided frame at the free virtual address
    errval_t err;
    // err = paging_alloc(st, buf, bytes, 1);
    size_t ret_size;
    err = paging_region_map(&st->meta_region,bytes,buf,&ret_size);
    ON_ERR_PUSH_RETURN(err, LIB_ERR_VSPACE_MAP);

    err = paging_map_fixed_attr(st, (lvaddr_t) *buf, frame, bytes, flags);
    ON_ERR_PUSH_RETURN(err, LIB_ERR_PMAP_DO_MAP);

    return SYS_ERR_OK;

}

errval_t slab_refill_no_pagefault(struct slab_allocator *slabs, struct capref frame,
                                  size_t minbytes)
{
    assert(slabs != NULL);
    errval_t err;
    const size_t bytes = ROUND_UP(minbytes, BASE_PAGE_SIZE);
    size_t size;
    err = frame_create(frame, bytes, &size);
    ON_ERR_PUSH_RETURN(err, LIB_ERR_FRAME_CREATE);

    void* buf = NULL;
    err = paging_map_frame_complete(get_current_paging_state(), &buf, frame, NULL, NULL);
    ON_ERR_RETURN(err);

    slab_grow(slabs, buf, size);
    return SYS_ERR_OK;
}





/**
 * \brief Helper function that allocates a slot and
 *        creates a aarch64 page table capability for a certain level
 */
static errval_t pt_alloc(struct paging_state * st, enum objtype type, 
                         struct capref *ret) 
{
    errval_t err;
    err = st->slot_alloc->alloc(st->slot_alloc, ret);
    if (err_is_fail(err)) {
        debug_printf("slot_alloc failed: %s\n", err_getstring(err));
        return err;
    }
    err = vnode_create(*ret, type);
    if (err_is_fail(err) && err_no(err) != LIB_ERR_CAP_DESTROY) {
        debug_printf("vnode_create failed: %s\n", err_getstring(err));
        return err;
    }
    return SYS_ERR_OK;
}


/**
 * \brief check whether the slab allocator for the shadow page table needs a refill.
 * 
 * As in order to refill the slab allocator we may need to map some pages, we need to
 * make sure that we always have some spare space and refill early enough.
 */
static errval_t paging_check_spt_refill(struct paging_state *st)
{
    if (slab_freecount(&st->mappings_alloc) < 10) {
        if (!st->mappings_alloc_is_refilling) {
            st->mappings_alloc_is_refilling = true;
            debug_printf("refilling slab alloc\n");
            {
                struct capref frameslot;
                errval_t err = st->slot_alloc->alloc(st->slot_alloc, &frameslot);
                if (err_is_fail(err)) {
                    st->mappings_alloc_is_refilling = false;
                    return err;
                }

                size_t refill_bytes = ROUND_UP(sizeof(struct mapping_table) * 32, BASE_PAGE_SIZE);

                err = slab_refill_no_pagefault(&st->mappings_alloc, frameslot, refill_bytes);
                if(err_is_fail(err)) {
                    DEBUG_ERR(err, "shadow page table slab alloc could not be refilled.");
                    st->mappings_alloc_is_refilling = false;
                    return err;
                }
            }
            st->mappings_alloc_is_refilling = false;
        }
    }
    return SYS_ERR_OK;
}


/**
 * \brief perform a lookup in the shadow page table structure
 * 
 * \param st the paging state containing the shadow page table to perform the lookup in
 * \param level either 0, 1, 2, or 3; specifies the level of the page table to look up
 * \param addr the address to resolve
 * \param create whether to create page tables that don't exist on the way.
 *               if this is false, the function returns NULL if the page table does not exist.
 * \param ret return parameter for a pointer to the mapping_table shadowing the requested level
 *            page table in the resolve path for addr
 */
errval_t paging_spt_find(struct paging_state *st, int level, lvaddr_t vaddr, bool create, struct mapping_table **ret)
{
    assert(st != NULL);
    assert(level >= 0 && level < 4);

    errval_t err;

    // indices into the four page tables
    const uint64_t pt_index[4] = {
        (vaddr >> (12 + 3 * 9)) & 0x1FF,
        (vaddr >> (12 + 2 * 9)) & 0x1FF,
        (vaddr >> (12 + 1 * 9)) & 0x1FF,
        (vaddr >> (12 + 0 * 9)) & 0x1FF,
    };

    // corresponding shadow page tables
    struct mapping_table *shadow_tables[4] = {
        &st->map_l0,
        NULL, NULL, NULL
    };

    const static enum objtype pt_types[4] = {
        ObjType_VNode_AARCH64_l0,
        ObjType_VNode_AARCH64_l1,
        ObjType_VNode_AARCH64_l2,
        ObjType_VNode_AARCH64_l3
    };
    

    // walking shadow page table
    for (int i = 0; i < 3; i++) {
        struct mapping_table *table = shadow_tables[i];
        struct mapping_table *child = table->children[pt_index[i]];
        struct capref mapping_child = table->mapping_caps[pt_index[i]];
        enum objtype child_type = pt_types[i + 1];
        int index = pt_index[i];

        if (child == NULL) {
            // table does not exist

            if (!capref_is_null(mapping_child)) {
                // superpage mapping in place at this address
                return LIB_ERR_PMAP_NO_VNODE_BUT_SUPERPAGE;
            }

            if (!create) {
                if (ret != NULL) {
                    *ret = NULL;
                }
                return SYS_ERR_OK;
            }

            // if there exists no table at this position, create it
            struct capref pt_cap;
            struct capref mapping_cap;
            err = st->slot_alloc->alloc(st->slot_alloc, &mapping_cap);
            ON_ERR_PUSH_RETURN(err, LIB_ERR_SLOT_ALLOC);

            err = pt_alloc(st, child_type, &pt_cap);
            ON_ERR_PUSH_RETURN(err, LIB_ERR_PMAP_ALLOC_VNODE);

            err = vnode_map(table->pt_cap, pt_cap, index, VREGION_FLAGS_READ, 0, 1, mapping_cap);
            ON_ERR_PUSH_RETURN(err, LIB_ERR_PMAP_DO_MAP);

            child = slab_alloc(&st->mappings_alloc);
            NULLPTR_CHECK(child, LIB_ERR_SLAB_ALLOC_FAIL);

            init_mapping_table(child);

            lvaddr_t vnode_start_address = vaddr & ~((1 << (12 + (3 - i) * 9)) - 1);
            child->region = paging_region_lookup(st, vnode_start_address);

            child->pt_cap = pt_cap;
            table->children[index] = child;
            table->mapping_caps[index] = mapping_cap;


            // check whether we need to refill the slab allocator for the shadow page table (st->mappings_alloc)
            paging_check_spt_refill(st);
        }
        shadow_tables[i + 1] = child;

        if (i + 1 == level) {
            if (ret != NULL) {
                *ret = child;
            }
            return SYS_ERR_OK;
        }
    }

    return LIB_ERR_PMAP_FIND_VNODE;
}

/**
 * \brief like paging_map_fixed_attr, but you can specify an offset into the frame at which to map
 */
errval_t paging_map_fixed_attr(struct paging_state *st, lvaddr_t vaddr,
                               struct capref frame, size_t bytes, int flags)
{
    assert(st != NULL);

    // only able to map whole pages, round up to next page boundary
    bytes = ROUND_UP(bytes, BASE_PAGE_SIZE);

    errval_t err;

    // perform multiple mappings in order to map whole frame
    for (size_t i = 0; i < bytes / BASE_PAGE_SIZE; i++) {

        // start address of page to map
        lvaddr_t page_start_addr = vaddr + i * BASE_PAGE_SIZE;

        size_t size_left = bytes - i * BASE_PAGE_SIZE;

        // check if we can map a superpage
        bool map_large_page = (page_start_addr % LARGE_PAGE_SIZE) == 0 && size_left >= LARGE_PAGE_SIZE;

        int page_level = map_large_page ? 2 : 3;

        struct mapping_table *table;
        err = paging_spt_find(st, page_level, page_start_addr, true, &table);
        ON_ERR_PUSH_RETURN(err, LIB_ERR_PMAP_SHADOWPT_LOOKUP);

        int pt_index = map_large_page ?
                (page_start_addr >> 21) & 0x1FF
                :
                (page_start_addr >> 12) & 0x1FF;

        if (!capcmp(table->mapping_caps[pt_index], NULL_CAP)) {
            DEBUG_PRINTF("attempting to map already mapped page\n");
            return LIB_ERR_PMAP_ADDR_NOT_FREE;
        }

        struct capref mapping;
        err = st->slot_alloc->alloc(st->slot_alloc, &mapping);
        if (err_is_fail(err)) {
            DEBUG_ERR(err, "couldn't alloc slot\n");
        }

        //debug_printf("mapping at: %lx\n", page_start_addr);
        err = vnode_map (
            table->pt_cap,
            frame,
            pt_index,
            flags,
            i * BASE_PAGE_SIZE,
            1,
            mapping
        );
        ON_ERR_RETURN(err);
        //debug_printf("mapped at: %lx\n", page_start_addr);

        table->mapping_caps[pt_index] = mapping;
        if (map_large_page) {
            i += LARGE_PAGE_SIZE / BASE_PAGE_SIZE - 1;
        }
    }

    return SYS_ERR_OK;
}


/**
 * \brief unmap a user provided frame, and return the VA of the mapped
 *        frame in `buf`.
 * NOTE: Implementing this function is optional.
 */
errval_t paging_unmap(struct paging_state *st, const void *region)
{
    assert(st != NULL);
    return LIB_ERR_NOT_IMPLEMENTED;
}
