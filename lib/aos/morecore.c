/**
 * \file
 * \brief Morecore implementation for malloc
 */

/*
 * Copyright (c) 2007, 2008, 2009, 2010, 2011, 2019 ETH Zurich.
 * Copyright (c) 2014, HP Labs.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetstr. 6, CH-8092 Zurich. Attn: Systems Group.
 */

#include <aos/aos.h>
#include <aos/core_state.h>
#include <aos/morecore.h>
#include <stdio.h>

typedef void *(*morecore_alloc_func_t)(size_t bytes, size_t *retbytes);
extern morecore_alloc_func_t sys_morecore_alloc;

typedef void (*morecore_free_func_t)(void *base, size_t bytes);
extern morecore_free_func_t sys_morecore_free;

// this define makes morecore use an implementation that just has a static
// 16MB heap.
// TODO (M4): use a dynamic heap instead,
// #define USE_STATIC_HEAP

#ifdef USE_STATIC_HEAP

// dummy mini heap (16M)

#define HEAP_SIZE (1<<24)

static char mymem[HEAP_SIZE] = { 0 };
static char *endp = mymem + HEAP_SIZE;

/**
 * \brief Allocate some memory for malloc to use
 *
 * This function will keep trying with smaller and smaller frames till
 * it finds a set of frames that satisfy the requirement. retbytes can
 * be smaller than bytes if we were able to allocate a smaller memory
 * region than requested for.
 */
static void *morecore_alloc(size_t bytes, size_t *retbytes)
{   

    debug_printf("More core alloc called:\n");
    struct morecore_state *state = get_morecore_state();

    size_t aligned_bytes = ROUND_UP(bytes, sizeof(Header));
    void *ret = NULL;
    if (state->freep + aligned_bytes < endp) {
        ret = state->freep;
        state->freep += aligned_bytes;
    }
    else {
        aligned_bytes = 0;
    }
    *retbytes = aligned_bytes;
    return ret;
}

static void morecore_free(void *base, size_t bytes)
{
    return;
}

errval_t morecore_init(size_t alignment)
{
    struct morecore_state *state = get_morecore_state();

    debug_printf("initializing static heap\n");

    thread_mutex_init(&state->mutex);

    state->freep = mymem;
    debug_printf("Static heap is initialized at mem: %lx to : %lx\n",mymem,&mymem[HEAP_SIZE - 1]);
    sys_morecore_alloc = morecore_alloc;
    sys_morecore_free = morecore_free;
    return SYS_ERR_OK;
}

errval_t morecore_reinit(void)
{
    return SYS_ERR_OK;
}

#else
// dynamic heap using lib/aos/paging features

/**
 * \brief Allocate some memory for malloc to use
 *
 * This function will keep trying with smaller and smaller frames till
 * it finds a set of frames that satisfy the requirement. retbytes can
 * be smaller than bytes if we were able to allocate a smaller memory
 * region than requested for.
 *
 * \param bytes Size in bytes of the part to be allocated
 * \param retbytes The actual size of the allocated part
 * \returns The address to the start to the allocated part
 */
static void *morecore_alloc(size_t bytes, size_t *retbytes)
{
    struct morecore_state *state = get_morecore_state();
    assert(state != NULL);

    size_t aligned_bytes = ROUND_UP(bytes, sizeof(Header));     // TODO: Understand this line
    void * retbuf = NULL;
    size_t ret_size;

    errval_t err = paging_region_map(state->region, aligned_bytes, &retbuf, &ret_size);

    if(err_is_fail(err) || ret_size < bytes){
        return NULL;
    }

    *retbytes = aligned_bytes;
    return retbuf;
}

/**
 * \brief Used to free/unallocate parts of the allocated parts in the region.
 *
 * \param base Start address of the part to be freed
 * \param bytes Size of the part to be freed
 */
static void morecore_free(void *base, size_t bytes)
{
    struct morecore_state *state = get_morecore_state();
    assert(state != NULL);

    errval_t err = paging_region_unmap(state->region, (lvaddr_t) base, bytes);
    ON_ERR_NO_RETURN(err);
}

/**
 * \brief Initializing the morecore state by filling the morecore struct
 *
 * \param alignment
 * \returns The error in the process
 */
errval_t morecore_init(size_t alignment)
{
    struct morecore_state *state = get_morecore_state();
    thread_mutex_init(&state->mutex);
    struct paging_state *ps = get_current_paging_state();
    state->region = &ps->heap_region;

    sys_morecore_alloc = morecore_alloc;
    sys_morecore_free = morecore_free;
    return SYS_ERR_OK;
}


errval_t morecore_reinit(void)
{
    USER_PANIC("NYI \n");
    return SYS_ERR_OK;
}

#endif

Header *get_malloc_freep(void);
Header *get_malloc_freep(void)
{
    return get_morecore_state()->header_freep;
}
