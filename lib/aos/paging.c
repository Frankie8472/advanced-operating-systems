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
#include "threads_priv.h"
#include <stdio.h>
#include <string.h>

static struct paging_state current;

// struct thread;


/**
 * \brief Helper function that allocates a slot and
 *        creates a aarch64 page table capability for a certain level
 */
static errval_t pt_alloc(struct paging_state * st, enum objtype type, 
                         struct capref *ret) 
{
    errval_t err;
    //err = slot_alloc(ret);
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

__attribute__((unused)) static errval_t pt_alloc_l1(struct paging_state * st, struct capref *ret)
{
    return pt_alloc(st, ObjType_VNode_AARCH64_l1, ret);
}

__attribute__((unused)) static errval_t pt_alloc_l2(struct paging_state * st, struct capref *ret)
{
    return pt_alloc(st, ObjType_VNode_AARCH64_l2, ret);
}

__attribute__((unused)) static errval_t pt_alloc_l3(struct paging_state * st, struct capref *ret) 
{
    return pt_alloc(st, ObjType_VNode_AARCH64_l3, ret);
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
    // occurs and keeps track of the virtual address space.
    // errval_t err;
    static char init_mem[SLAB_STATIC_SIZE(32, sizeof(struct mapping_table))];
    slab_init(&st->mappings_alloc, sizeof(struct mapping_table), NULL);
    slab_grow(&st->mappings_alloc, init_mem, sizeof(init_mem));
    st->mappings_alloc_is_refilling = false;
    st->map_l0.pt_cap = pdir;
    st->slot_alloc = ca;

    static char init_mem2[SLAB_STATIC_SIZE(32, sizeof(struct paging_region))];
    slab_init(&st->region_alloc, sizeof(struct paging_region), NULL);
    slab_grow(&st->region_alloc, init_mem2, sizeof(init_mem2));

    struct paging_region *pr = slab_alloc(&st->region_alloc);
    pr->base_addr=VADDR_OFFSET;
    pr->current_addr=VADDR_OFFSET;
    pr->region_size=0xffffffffffffL-VADDR_OFFSET;
    pr->flags=VREGION_FLAGS_GUARD;
    pr->next=NULL;
    pr->prev=NULL;

    st->head = pr;

    paging_region_init(st, &st->meta_region, 1L<<43, VREGION_FLAGS_READ_WRITE);
    paging_region_init(st, &st->heap_region, 1L<<42, VREGION_FLAGS_READ_WRITE);
    paging_region_init(st, &st->stack_region, 1L<<10, VREGION_FLAGS_READ_WRITE);

    static char init_mem_guards[SLAB_STATIC_SIZE(32, sizeof(struct stack_guard))];
    slab_init(&st -> guards_alloc,sizeof(struct stack_guard),NULL);
    slab_grow(&st->guards_alloc, init_mem_guards, sizeof(init_mem_guards));
    // struct capref guard_cap;
    // err = page_table_walk(st,stack_bottom,&guard_cap);
    // if(err_is_fail(err)){
    //     DEBUG_ERR(err,"Failed to walk page table for stack capability\n");
    // }


    
    // struct capref cap;
    // // size_t size = BA
    // size_t retbytes;
    // err = frame_alloc(&cap,BASE_PAGE_SIZE,&retbytes);
    // if(err_is_fail(err)){
    //     debug_printf("Failed to alloc frame in paging init\n");

    // }



    // dispatcher_handle_t handle = curdispatcher();
    // struct dispatcher_generic* disp = get_dispatcher_generic(handle);
    // struct thread* curr_thread = disp -> current;
    // struct thread* curr_thread = curr_thread;
 

    // debug_printf("Current stack pointer: %ld\n",sp);
    // curr_thread -> stack = (void *) VADDR_OFFSET + (1L<<43) + (1L<<42) + (1L << 10);
    // curr_thread -> paging_region_init
    // errval_t err;
    // struct capref cap;
    // // size_t size = BA
    // size_t retbytes;
    // err = frame_alloc(&cap,BASE_PAGE_SIZE,&retbytes);
    // if(err_is_fail(err)){
    //     debug_printf("Failed to alloc frame in paging init\n");

    // }
    // err = paging_map_fixed_attr_with_offset(st,curr_thread -> stack,cap,)

    // curre_thread -> stack = 
    /*
    size_t meta_data_size = 1L<<43; // meta data size, around 8 TB
    paging_region_init_fixed(st,&st -> meta_region,start_vaddr,meta_data_size,VREGION_FLAGS_READ_WRITE);
    start_vaddr += meta_data_size;

    size_t heap_size = 1L<<42; // heap size of around 4 TB 
    paging_region_init_fixed(st,&st -> heap_region,start_vaddr,heap_size,VREGION_FLAGS_READ_WRITE);
    start_vaddr += heap_size;


    st->current_address = start_vaddr;

    debug_printf("Settting init_state at start_vaddr:=%lx\n",start_vaddr);*/
    return SYS_ERR_OK;
}

/**
 * TODO(M2): Implement this function.
 * TODO(M4): Improve this function.
 * \brief Initialize the paging_state struct for the paging state
 *        of a child process.
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
    // occurs and keeps track of the virtual address space.
    assert(st != NULL);

    memset(st, 0, sizeof(struct paging_state));
    st->current_address = start_vaddr;
    st->map_l0.pt_cap = pdir;
    st->slot_alloc = ca;

    errval_t err;
    struct capref frame;
    size_t actual_size;
    err = frame_alloc(&frame, ROUND_UP(sizeof(struct mapping_table) * 32, BASE_PAGE_SIZE), &actual_size);

    void *slab_memory;
    err = paging_map_frame_complete(get_current_paging_state(), &slab_memory, frame, NULL, NULL);

    slab_init(&st->mappings_alloc, sizeof(struct mapping_table), NULL);
    slab_grow(&st->mappings_alloc, slab_memory, actual_size);

    return SYS_ERR_OK;
}

bool isIn(void* addr,struct paging_region pr){
    return ((lvaddr_t) addr) >  pr.base_addr && ((lvaddr_t) addr) < (pr.base_addr + pr.region_size);
}

bool is_in_guard(void* addr, struct stack_guard* sg){
return (lvaddr_t) addr >  sg -> stack_bottom && (lvaddr_t) addr < (sg -> stack_bottom + 8 * 1024);
}

bool is_in_guards(void* addr, struct paging_state* ps){
    struct stack_guard* it = ps -> guards;
    if(it == NULL){
        return false;
    }else {
        while(it != NULL){
            if(is_in_guard(addr,it)){return true;}
            it = it -> next;
        }
        return false;
    }
}

void add_stack_guard(struct paging_state* ps, uintptr_t id, lvaddr_t stack_bottom){
    struct stack_guard* sg = slab_alloc(&ps -> guards_alloc);
    sg -> stack_bottom = stack_bottom;
    sg -> thread_id = id;
    debug_printf("Stack guard create with stack guard bottom = %lx\n",stack_bottom);
    if(ps -> guards == NULL){
        ps -> guards = sg;
        return;
    }
    else{
        struct stack_guard* it = ps -> guards;
        while(it -> next != NULL){it = it -> next;}
        it -> next = sg;
        return;
    }
}

static void page_fault_handler(enum exception_type type,int subtype,void *addr,arch_registers_state_t *regs){
    errval_t err;
    debug_printf("handling pagefault!\n");
    // debug_printf("type: %d\n", type);
    // debug_printf("subtype: %d\n", subtype);
    debug_printf("addr: 0x%" PRIxLPADDR "\n", addr);
    // debug_printf("ip: 0x%" PRIxLPADDR "\n", regs->named.pc);
    
    
    if(type == EXCEPT_PAGEFAULT){

        struct paging_state* ps = get_current_paging_state();
        if(addr  == 0){
            debug_printf("Core dumped (Segmentation fault)\n");
        }
        if(is_in_guards(addr,ps)){
            debug_printf("Core dumped (Stack overflow)\n");
            debug_printf("Exiting thread\n");
            thread_exit(0);
            return;
        }
    
        assert(ps != NULL);
        if(isIn(addr,ps -> heap_region)){
            // debug_printf("Page fault inside the heap!\n");
            struct capref frame;
            size_t retbytes;
            err = frame_alloc(&frame,BASE_PAGE_SIZE,&retbytes);
            if(err_is_fail(err)){
                DEBUG_ERR(err,"Failed to allocate a new physical frame inside the pagefault handler\n");
            }
            lvaddr_t vaddr= ROUND_DOWN((lvaddr_t) addr, BASE_PAGE_SIZE);
            err = paging_map_fixed_attr(ps,vaddr,frame,BASE_PAGE_SIZE,VREGION_FLAGS_READ_WRITE);
                        if(err_is_fail(err)){
                DEBUG_ERR(err,"Failed to map frame in pagefault handler\n");
            }
            return;
        }



        if(isIn(addr,ps -> meta_region)){
            debug_printf("Page fault inside the meta region!\n");
            struct capref frame;
            size_t retbytes;
            err = frame_alloc(&frame,BASE_PAGE_SIZE,&retbytes);
            if(err_is_fail(err)){
                DEBUG_ERR(err,"Failed to allocate a new physical frame inside the pagefault handler\n");
            }
            lvaddr_t vaddr= ROUND_DOWN((lvaddr_t) addr, BASE_PAGE_SIZE);
            err = paging_map_fixed_attr(ps,vaddr,frame,BASE_PAGE_SIZE,VREGION_FLAGS_READ_WRITE);
            if(err_is_fail(err)){
                DEBUG_ERR(err,"Failed to map frame in pagefault handler\n");
            }
            return;
        }
    }
    debug_printf("Exiting thread\n");
    thread_exit(0);
    return;
}


// errval_t paging_init_stack(struct paging_state* ps){
//     errval_t err = SYS_ERR_OK;   
//     dispatcher_handle_t handle = curdispatcher();
//     struct dispatcher_generic* disp = get_dispatcher_generic(handle);
//     struct thread* curr_thread = disp -> current;
//     // lvaddr_t stack_bottom = (lvaddr_t) curr_thread -> stack;
//     // lvaddr_t stack_top = (lvaddr_t) curr_thread -> stack_top;

//     // st -> stack_region 
//     debug_printf("Current stack top: %d\n",curr_thread -> stack_top);
//     debug_printf("Current stack: %ld\n",curr_thread -> stack);

//     struct capref stack_cap_top;
//     size_t ret_bytes;
//     err = frame_alloc(&stack_cap_top,BASE_PAGE_SIZE,&ret_bytes);
//     if(err_is_fail(err)){
//         DEBUG_ERR(err,"Failed to alloc fram in paging_init_stack\n");
//     }

//     // struct capref stack_cap_bot;
//     // size_t ret_bytes;
//     // err = frame_alloc(&stack_cap_bot,BASE_PAGE_SIZE,&ret_bytes);
//     // if(err_is_fail(err)){
//     //     DEBUG_ERR(err,"Failed to alloc fram in paging_init_stack\n");
//     // }
//     // debug_print_paging_region(ps -> stack_region);
//     // lvaddr_t new_stack_top = (ps -> stack_region.base_addr) + (ps -> stack_region.region_size);
//     // // lvaddr_t new_stack_bottom = ps -> stack_region.base_addr;
//     // uint64_t  stack_base = ps -> stack_region.base_addr;
//     // uint64_t stack_size = ps -> stack_region.region_size;
//     // uint64_t stack_top = stack_base + stack_size;


//     // debug_printf("Current stack top: %lx\n",stack_top);
//     // debug_printf("Current stack size: %lx\n",stack_size);
//     // debug_printf("Current stack bot: %lx\n",ps -> stack_region.base_addr);


//     // err = paging_map_fixed_attr(ps,new_stack_top - BASE_PAGE_SIZE,stack_cap_top,BASE_PAGE_SIZE,VREGION_FLAGS_READ_WRITE);
//     // if(err_is_fail(err)){
//     //     DEBUG_ERR(err,"Failed paging_map_fixed_attr\n");
//     // }
//     // curr_thread -> stack_top = (void * ) stack_top;
//     // curr_thread -> stack = (void * ) stack_base;
//     // debug_printf("Current stack top: %lx\n",curr_thread -> stack_top);
//     // debug_printf("Current stack: %lx\n",curr_thread -> stack);
//     // arch_registers_state_t regs = curr_thread -> regs; 
//     // uint64_t sp = registers_get_sp(&regs);
//     // debug_printf("Current stack register: %ld\n",sp);
//     // registers_set_sp(&regs,stack_top);
//     // sp = registers_get_sp(&regs);
//     // debug_printf("Current stack register: %lx\n",sp);


//     // err = paging_map_fixed_attr(ps,(lvaddr_t)curr_thread -> stack,stack_cap_top,BASE_PAGE_SIZE,VREGION_FLAGS_READ_WRITE);
//     // if(err_is_fail(err)){
//     //     DEBUG_ERR(err,"Failed paging_map_fixed_attr\n");
//     // }


//     return err;
// }

/**
 * \brief This function initializes the paging for this domain
 * It is called once before main.
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


    debug_printf("paging_init\n");

    struct capref root_pagetable = {
        .cnode = cnode_page,
        .slot  = 0
    };

    errval_t err;
    static char new_stack[32 * 1024];
    // void* new_stack = malloc(BASE_PAGE_SIZE);

    exception_handler_fn handler = (exception_handler_fn) page_fault_handler;
    err = thread_set_exception_handler(handler,NULL,new_stack,new_stack + sizeof(new_stack),NULL,NULL);



    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to set exception handler in paging init\n");
    }


    err = paging_init_state(&current, VADDR_OFFSET, root_pagetable, get_default_slot_allocator()); 
    ON_ERR_PUSH_RETURN(err, LIB_ERR_VSPACE_INIT);

    set_current_paging_state(&current);
    err = slot_alloc_init();
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
    static char new_stack[32 * 1024];
    t -> exception_handler = page_fault_handler;
    t -> exception_stack = (void * )  new_stack;
    t -> exception_stack_top = (void * )  new_stack + sizeof(new_stack);
}   

/**
 * \brief Initialize a paging region in `pr`, such that it  starts
 * from base and contains size bytes.
 */
errval_t paging_region_init_fixed(struct paging_state *st, struct paging_region *pr,
                                  lvaddr_t base, size_t size, paging_flags_t flags)
{
    assert(st != NULL && pr != NULL);

    pr->base_addr = (lvaddr_t) base;
    pr->current_addr = pr->base_addr;
    pr->region_size = size;
    pr->flags = flags;

    //TODO(M2): Add the region to a datastructure and ensure paging_alloc
    //will return non-overlapping regions.
    // TODO inspect this; maybe replace with smarter allocating algorithm
    if (st->current_address < pr->base_addr + pr->region_size) {
        st->current_address = pr->base_addr + pr->region_size;
    }
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
    /*
    void *base;
    errval_t err = paging_alloc(st, &base, size, alignment);
    ON_ERR_PUSH_RETURN(err, LIB_ERR_VSPACE_MMU_AWARE_INIT);

    err = paging_region_init_fixed(st, pr, (lvaddr_t)base, size, flags);
    ON_ERR_PUSH_RETURN(err, LIB_ERR_PMAP_DO_MAP);
    */
    return SYS_ERR_OK;
}

/**
 * \brief Initialize a paging region in `pr`, such that it contains at least
 * size bytes.
 *
 * This function gets used in some of the code that is responsible
 * for allocating Frame (and other) capabilities.
 */
errval_t paging_region_init(struct paging_state *st, struct paging_region *pr,
                            size_t size, paging_flags_t flags)
{
    assert(st != NULL && pr != NULL);

    struct paging_region *cur = st->head;
    for(; cur != NULL && (cur->flags != VREGION_FLAGS_GUARD || cur->region_size < size); cur = cur->next);
    NULLPTR_CHECK(cur, LIB_ERR_VSPACE_MMU_AWARE_NO_SPACE);

    pr->region_size = size;
    pr->flags = flags;
    pr->next = cur;
    pr->prev = cur->prev;
    pr->base_addr = cur->base_addr;
    pr->current_addr = pr->base_addr;

    cur->base_addr += size;
    cur->current_addr = cur->base_addr;
    cur->prev = pr;
    cur->region_size -= size;

    if(pr->prev != NULL) {
        pr->prev->next = pr;
    }

    return SYS_ERR_OK;
    //return paging_region_init_aligned(st, pr, size, BASE_PAGE_SIZE, flags);
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
    err = paging_region_map(&st -> meta_region,bytes,buf,&ret_size);
    ON_ERR_PUSH_RETURN(err, LIB_ERR_VSPACE_MAP);

    err = paging_map_fixed_attr(st, (lvaddr_t) *buf, frame, bytes, flags);
    ON_ERR_PUSH_RETURN(err, LIB_ERR_PMAP_DO_MAP);

    return SYS_ERR_OK;

}

errval_t slab_refill_no_pagefault(struct slab_allocator *slabs, struct capref frame,
                                  size_t minbytes)
{
    assert(slabs != NULL);

    const size_t bytes = ROUND_UP(minbytes, BASE_PAGE_SIZE);
    size_t size;
    frame_create(frame, bytes, &size);
    void* buf = NULL;
    errval_t err = paging_map_frame_complete(get_current_paging_state(), &buf, frame, NULL, NULL);
    ON_ERR_RETURN(err);

    slab_grow(slabs, buf, size);
    return SYS_ERR_OK;
}



static errval_t paging_map_fixed_attr_with_offset(struct paging_state *st, lvaddr_t vaddr,
                               struct capref frame, gensize_t offset, size_t bytes, int flags);

errval_t paging_map_fixed_attr(struct paging_state *st, lvaddr_t vaddr,
                               struct capref frame, size_t bytes, int flags)
{
    return paging_map_fixed_attr_with_offset(st, vaddr, frame, 0, bytes, flags);
}

/**
 * \brief like paging_map_fixed_attr, but you can specify an offset into the frame at which to map
 */
static errval_t paging_map_fixed_attr_with_offset(struct paging_state *st, lvaddr_t vaddr,
                               struct capref frame, gensize_t offset, size_t bytes, int flags)
{
    /**
     * \brief map a user provided frame at user provided VA.
     * TODO(M1): Map a frame assuming all mappings will fit into one last level pt
     * TODO(M2): General case
     */
    assert(st != NULL);

    static size_t mapped = 0;
    mapped += BASE_PAGE_SIZE;
    if (mapped % (BASE_PAGE_SIZE * 1024) == 0)
        debug_printf("mapped %ld\n", mapped);

    // only able to map whole pages, round up to next page boundary
    bytes = ROUND_UP(bytes, BASE_PAGE_SIZE);

    // offset into frames must be page-aligned
    assert(offset % BASE_PAGE_SIZE == 0);

    uint64_t l0_offset = (vaddr >> (12 + 3 * 9)) & 0x1FF;
    uint64_t l1_offset = (vaddr >> (12 + 2 * 9)) & 0x1FF;
    uint64_t l2_offset = (vaddr >> (12 + 1 * 9)) & 0x1FF;
    uint64_t l3_offset = (vaddr >> (12 + 0 * 9)) & 0x1FF;

    struct mapping_table *shadow_table_l0 = &st->map_l0;
    struct mapping_table *shadow_table_l1 = NULL;
    struct mapping_table *shadow_table_l2 = NULL;
    struct mapping_table *shadow_table_l3 = NULL;

    errval_t err;

    shadow_table_l1 = shadow_table_l0->children[l0_offset];

    if (shadow_table_l1 == NULL) { // if there is no l1 pt in l0
        struct capref l1;
        struct capref l1_mapping;
        st->slot_alloc->alloc(st->slot_alloc, &l1_mapping);
        pt_alloc(st, ObjType_VNode_AARCH64_l1, &l1);

        //printf("mapping l1 node 0x%lx\n", l0_offset);
        err = vnode_map(shadow_table_l0->pt_cap, l1, l0_offset, VREGION_FLAGS_READ, 0, 1, l1_mapping);
        ON_ERR_RETURN(err);


        shadow_table_l1 = slab_alloc(&st->mappings_alloc);
        NULLPTR_CHECK(shadow_table_l1, LIB_ERR_SLAB_ALLOC_FAIL);

        init_mapping_table(shadow_table_l1);
        shadow_table_l1->pt_cap = l1;
        shadow_table_l0->children[l0_offset] = shadow_table_l1;
        shadow_table_l0->mapping_caps[l0_offset] = l1_mapping;
    }

    shadow_table_l2 = shadow_table_l1->children[l1_offset];

    if (shadow_table_l2 == NULL) { // if there is no l2 pt in l1
        struct capref l2;
        struct capref l2_mapping;
        st->slot_alloc->alloc(st->slot_alloc, &l2_mapping);
        pt_alloc(st, ObjType_VNode_AARCH64_l2, &l2);

        //printf("mapping l2 node 0x%lx\n", l1_offset);
        err = vnode_map(shadow_table_l1->pt_cap, l2, l1_offset, VREGION_FLAGS_READ, 0, 1, l2_mapping);
        ON_ERR_RETURN(err);

        shadow_table_l2 = slab_alloc(&st->mappings_alloc);
        NULLPTR_CHECK(shadow_table_l2, LIB_ERR_SLAB_ALLOC_FAIL);

        init_mapping_table(shadow_table_l2);
        shadow_table_l2->pt_cap = l2;
        shadow_table_l1->children[l1_offset] = shadow_table_l2;
        shadow_table_l1->mapping_caps[l1_offset] = l2_mapping;
    }

    shadow_table_l3 = shadow_table_l2->children[l2_offset];

    if (shadow_table_l3 == NULL) { // if there is no l3 pt in l2
        struct capref l3;
        struct capref l3_mapping;
        st->slot_alloc->alloc(st->slot_alloc, &l3_mapping);
        pt_alloc(st, ObjType_VNode_AARCH64_l3, &l3);

        //printf("mapping l3 node\n");
        err = vnode_map(shadow_table_l2->pt_cap, l3, l2_offset, VREGION_FLAGS_READ, 0, 1, l3_mapping);
        ON_ERR_RETURN(err);

        shadow_table_l3 = slab_alloc(&st->mappings_alloc);
        NULLPTR_CHECK(shadow_table_l3, LIB_ERR_SLAB_ALLOC_FAIL);

        init_mapping_table(shadow_table_l3);
        shadow_table_l3->pt_cap = l3;
        shadow_table_l2->children[l2_offset] = shadow_table_l3;
        shadow_table_l2->mapping_caps[l2_offset] = l3_mapping;
    }

    for (size_t i = 0; i < bytes / BASE_PAGE_SIZE; i++) {

        if (l3_offset + i >= PTABLE_ENTRIES) {
            size_t new_offset = i * BASE_PAGE_SIZE;
            //debug_printf("paging recursive: 0x%lx\n", vaddr + new_offset);

            // recursively call this function to map remaining pages
            return paging_map_fixed_attr_with_offset(st, vaddr + new_offset, frame, offset + new_offset, bytes - new_offset, flags);
        }

        // Milestone one assumption
        assert(l3_offset + i < PTABLE_ENTRIES);

        if (!capcmp(shadow_table_l3->mapping_caps[l3_offset + i], NULL_CAP)) {
            DEBUG_PRINTF("attempting to map already mapped page\n");
            return LIB_ERR_PMAP_ADDR_NOT_FREE;
        }

        struct capref mapping;
        st->slot_alloc->alloc(st->slot_alloc, &mapping);

        //debug_printf("mapping frame at off: 0x%x\n", i * BASE_PAGE_SIZE + offset);
        err = vnode_map (
            shadow_table_l3->pt_cap,
            frame,
            l3_offset + i,
            flags,
            i * BASE_PAGE_SIZE + offset,
            1,
            mapping
        );
        ON_ERR_RETURN(err);

        //debug_printf("mapped frame at off: 0x%x\n", l3_offset + i);
        shadow_table_l3->mapping_caps[l3_offset + i] = mapping;
    }

    if (slab_freecount(&st->mappings_alloc) < 10) {
        if (!st->mappings_alloc_is_refilling) {
            st->mappings_alloc_is_refilling = true;
            {
                struct capref frameslot;
                err = st->slot_alloc->alloc(st->slot_alloc, &frameslot);
                ON_ERR_RETURN(err);

                size_t refill_bytes = ROUND_UP(sizeof(struct mapping_table) * 32, BASE_PAGE_SIZE);

                err = slab_refill_no_pagefault(&st->mappings_alloc, frameslot, refill_bytes);
                if(err_is_fail(err)) {
                    DEBUG_ERR(err, "shadow page table slab alloc could not be refilled.");
                    return err;
                }
            }
            st->mappings_alloc_is_refilling = false;
        }
    }
    else {
        //debug_printf("no need to refill\n");
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
