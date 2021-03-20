/**
 * \file
 * \brief A library for managing physical memory (i.e., caps)
 */

#include <mm/mm.h>
#include <aos/debug.h>
#include <aos/solution.h>


const size_t SLAB_REFILL_THRESHOLD = 6;

/**
 * \brief manages slot allocation for mm-internal operations
 */
static errval_t mm_slot_alloc(struct mm *mm, struct capref *ret)
{
    return mm->slot_alloc_priv(mm->slot_alloc_inst, 1, ret);
    /*if (mm->initialized_slot) {
        struct slot_allocator* da = get_default_slot_allocator();
        return da->alloc(da, ret);
    }
    else {
        return mm->slot_alloc_priv(mm->slot_alloc_inst, 1, ret);
    }*/
}

/**
 * \brief Initializes an mmnode to the given MM allocator instance data
 *
 * \param mm Pointer to MM allocator instance data
 * \param objtype Type of the allocator and its nodes
 * \param slab_refill_func Pointer to function to call when out of memory (or NULL)
 * \param slot_alloc_func Slot allocator for allocating cspace
 * \param slot_refill_func Slot allocator refill function
 * \param slot_alloc_inst Opaque instance pointer for slot allocator
 */
errval_t mm_init(struct mm *mm, enum objtype objtype,
                     slab_refill_func_t slab_refill_func,
                     slot_alloc_t slot_alloc_func,
                     slot_refill_t slot_refill_func,
                     void *slot_alloc_inst)
{
    if (mm == NULL) {
        DEBUG_ERR(MM_ERR_NOT_FOUND, "mm.c/mm_init: mm is null");
        return MM_ERR_NOT_FOUND;
    }
    slab_init(&mm->slabs, sizeof(struct mmnode), slab_refill_func);
    mm->head = NULL;
    mm->objtype = objtype;
    mm->slot_alloc_priv = slot_alloc_func;
    mm->slot_refill = slot_refill_func;
    mm->slot_alloc_inst = slot_alloc_inst;
    mm->initialized_slot = false;
    mm->refilling = false;
    mm->stats_bytes_max = 0;
    mm->stats_bytes_available = 0;
    return SYS_ERR_OK;
}

/**
 * \brief Destroys an MM allocator instance data with all its nodes and capabilities
 *
 * \param mm Pointer to MM allocator instance data
 */
void mm_destroy(struct mm *mm)
{
    errval_t err;
    if (mm == NULL) {
        DEBUG_ERR(MM_ERR_NOT_FOUND, "mm.c/mm_destroy: mm is null");
        return;
    }

    struct mmnode *cm, *nm;
    for (cm = mm->head; cm != NULL; cm = nm) {
        for (nm = cm->next; nm != NULL && capcmp(cm->cap.cap, nm->cap.cap); nm = cm->next) {
            cm->next = nm->next;
            slab_free(&(mm->slabs), nm);
        }
        err = cap_revoke(cm->cap.cap);
        if (err) {
            DEBUG_ERR(LIB_ERR_REMOTE_REVOKE, "mm.c/mm_destroy: cap_revoke error");
            return;
        }

        err = cap_destroy(cm->cap.cap);
        if (err) {
            DEBUG_ERR(LIB_ERR_CAP_DESTROY, "mm.c/mm_destroy: cap_destroy error");
            return;
        }

        slab_free(&(mm->slabs), cm);
    }
}

/**
 * \brief simply insert a new node at the front of our linked list structure
 */
static void insert_node_as_head(struct mm *mm, struct mmnode *node)
{
    struct mmnode *old_head = mm->head;

    if (old_head != NULL) {
        old_head->prev = node;
    }
    mm->head = node;
    node->next = old_head;
    node->prev = NULL;
}

// TODO: maybe rewrite with less pointer clusterfuck
static errval_t split_node(struct mm *mm, struct mmnode *node, size_t offset, struct mmnode **a, struct mmnode **b)
{
    struct mmnode *new_node = slab_alloc(&mm->slabs);
    if (!new_node) {
        return LIB_ERR_SLAB_ALLOC_FAIL;
    }

    *a = node;
    *b = new_node;

    **b = **a;
    (*b)->base = (*a)->base + offset;
    (*b)->size = (*a)->size - offset;

    (*a)->size = offset;

    (*b)->prev = *a;
    (*a)->next = *b;

    if ((*b)->next != NULL) {
        (*b)->next->prev = *b;
    }

    return SYS_ERR_OK;
}

/**
 * \brief Adds an mmnode to the given MM allocator instance data
 *
 * \param mm Pointer to MM allocator instance data
 * \param cap Capability to the given RAM space
 * \param base Start_add of the RAM to allocate
 * \param size Amount of RAM to allocate, in bytes
 */
static void mm_check_refill(struct mm *mm)
{
    if (!mm->refilling) {
        int freec = slab_freecount(&mm->slabs);
        // if we have less slabs left than a certain threshold,
        // we preemptively refill.
        // This is done in case we need to allocate some slabs during the refilling
        if (freec <= SLAB_REFILL_THRESHOLD) {
            mm->refilling = true;
            slab_default_refill(&mm->slabs);
            mm->refilling = false;
        }
    }
}

errval_t mm_add(struct mm *mm, struct capref cap, genpaddr_t base, size_t size)
{
    struct mmnode *new_node = slab_alloc(&mm->slabs);

    insert_node_as_head(mm, new_node);

    new_node->type = NodeType_Free;
    new_node->cap = (struct capinfo) {
        .cap = cap,
        .base = base,
        .size = size
    };
    new_node->base = base;
    new_node->size = size;
    mm->stats_bytes_available += size;
    mm->stats_bytes_max += size;

    //DEBUG_PRINTF("memory region added!\n");
    return SYS_ERR_OK;
}

/**
 * \brief Allocates aligned memory in the form of a RAM capability
 *
 * \param mm Pointer to MM allocator instance data
 * \param size Amount of RAM to allocate, in bytes
 * \param alignment Alignment of RAM to allocate slot used for the cap in #ret, if any
 * \param retcap Pointer to capref struct, filled-in with allocated cap location
 */
errval_t mm_alloc_aligned(struct mm *mm, size_t size, size_t alignment, struct capref *retcap)
{
    if (alignment == 0) {
        DEBUG_PRINTF("invalid alignment of 0, using 1 instead");
        alignment = 1;
    }

    mm_slot_alloc(mm, retcap);

    struct mmnode *node = mm->head;

    errval_t err = SYS_ERR_OK;

    while(node != NULL) {
        if (node->type == NodeType_Free && node->size >= size) {
            struct mmnode *a, *b;

            if (node->base % alignment != 0) {
                uint64_t offset = alignment - (node->base % alignment);
                if (node->size - offset < size) {
                    node = node->next;
                    continue;
                }
                err = split_node(mm, node, offset, &a, &b);
                if (err_is_fail(err)) {
                    DEBUG_ERR(err, "could not align memory");
                    return err;
                }
                node = b;
            }

            err = cap_retype(*retcap,
                node->cap.cap,
                node->base - node->cap.base,
                mm->objtype,
                size,
                1
            );

            if (err_is_fail(err)) {
                DEBUG_ERR(err, "could not retype region cap");
                return err;
            }

            if (size < node->size) {
                err = split_node(mm, node, size, &a, &b);
                if (err_is_fail(err)) {
                    DEBUG_ERR(err, "error splitting mmnodes");
                    return err;
                }
                node = a;
            }

            node->type = NodeType_Allocated;
            mm->stats_bytes_available -= node->size;
            goto ok_refill;
        }
        node = node->next;
    }

    //printf("could not allocate a frame!\n");
    return LIB_ERR_RAM_ALLOC_FIXED_EXHAUSTED;
    mm->initialized_slot = true;
ok_refill:
    mm_check_refill(mm);
    return SYS_ERR_OK;
}

errval_t mm_alloc(struct mm *mm, size_t size, struct capref *retcap)
{
    return mm_alloc_aligned(mm, size, BASE_PAGE_SIZE, retcap);
}


/**
 * \brief Tries to merge an mmnode with its right neighbour.
 *        The merging only happens if they are adjacent and both free.
 *
 * \param mm Pointer to MM allocator instance data
 * \param mmnode Pointer to the mmnode
 */


static bool coalesce(struct mm *mm, struct mmnode *node)
{
    if (node == NULL) {
        return false;
    }

    struct mmnode *right = node->next;
    if (right == NULL) {
        return false;
    }
    if (!capcmp(right->cap.cap, node->cap.cap)) {
        return false;
    }
    if (node->type == NodeType_Free && right->type == NodeType_Free) {
        assert(node->base + node->size == right->base);

        node->size += right->size;
        node->next = right->next;

        if (node->next != NULL) {
            node->next->prev = node;
        }

        slab_free(&mm->slabs, right);
        return true;
    }
    return false;
}

/**
 * \brief Freeing allocated RAM and associated capability
 *
 * \param mm Pointer to MM allocator instance data
 * \param cap The capability fot the allocated RAM
 * \param base The start_addr for allocated RAM the cap is for
 * \param size The size of the allocated RAM the capability is for
 */
errval_t mm_free(struct mm *mm, struct capref cap, genpaddr_t base, gensize_t size)
{
    struct mmnode* node = mm->head;
    while(node != NULL) {
        if (node->base == base && node->size == size) {
            node->type = NodeType_Free;
            errval_t err = cap_destroy(cap);
            if (err_is_fail(err)) {
                DEBUG_ERR(err, "could not destroy ram cap");
                return err;
            }

            mm->stats_bytes_available += size;

            coalesce(mm, node);
            coalesce(mm, node->prev);

            return SYS_ERR_OK;
        }
        node = node->next;
    }
    return LIB_ERR_RAM_ALLOC_WRONG_SIZE;
}


void print_mm_state(struct mm *mm)
{
    for (struct mmnode* node = mm->head; node != NULL; node = node->next) {
        if (node->type == NodeType_Free) {
            printf("free   ");
        }
        else {
            printf("unfree ");
        }
        printf("mmnode with base 0x%x size 0x%x\n", node->base, node->size);
    }
}
