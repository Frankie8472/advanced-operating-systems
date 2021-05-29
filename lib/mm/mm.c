/**
 * \file
 * \brief A library for managing physical memory (i.e., caps)
 */

#include <mm/mm.h>
#include <aos/debug.h>
#include <aos/solution.h>
#include <aos/domain.h>

const size_t SLAB_REFILL_THRESHOLD = 7;

/**
 * \brief manages slot allocation for mm-internal operations
 *
 * \param mm Pointer to MM allocator instance data
 * \param ret Pointer to empty capability
 *
 * \returns err errorcode
 */
static errval_t mm_slot_alloc(struct mm *mm, struct capref *ret)
{
    assert(mm != NULL);

    return mm->slot_alloc_priv(mm->slot_alloc_inst, 1, ret);
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
 *
 * \returns err Error code
 */
errval_t mm_init(struct mm *mm, enum objtype objtype,
                     slab_refill_func_t slab_refill_func,
                     slot_alloc_t slot_alloc_func,
                     slot_refill_t slot_refill_func,
                     void *slot_alloc_inst)
{
    assert(mm != NULL);

    slab_init(&mm->slabs, sizeof(struct mmnode), slab_refill_func);
    mm->head = NULL;
    mm->free_head = NULL;
    mm->free_last = NULL;
    mm->objtype = objtype;
    mm->slot_alloc_priv = slot_alloc_func;
    mm->slot_refill = slot_refill_func;
    mm->slot_alloc_inst = slot_alloc_inst;
    mm->initialized_slot = false;
    mm->refilling = false;
    mm->stats_bytes_max = 0;
    mm->stats_bytes_available = 0;

    thread_mutex_init(&mm->mutex);
    return SYS_ERR_OK;
}

/**
 * \brief Destroys an MM allocator instance data with all its nodes and capabilities
 *
 * \param mm Pointer to MM allocator instance data
 */
void mm_destroy(struct mm *mm)
{
    assert(mm != NULL);

    errval_t err;

    struct mmnode *cm, *nm;
    for (cm = mm->head; cm != NULL; cm = nm) {
        for (nm = cm->next; nm != NULL && capcmp(cm->cap.cap, nm->cap.cap); nm = cm->next) {
            cm->next = nm->next;
            slab_free(&(mm->slabs), nm);
        }
        err = cap_revoke(cm->cap.cap);
        ON_ERR_NO_RETURN(err);

        err = cap_destroy(cm->cap.cap);
        ON_ERR_NO_RETURN(err);

        slab_free(&(mm->slabs), cm);
    }
}

/**
 * \brief Add a node to the list of free nodes.
 *
 * \param mm Pointer to MM allocator instance data.
 * \param node Node to add to the list of free nodes.
 */
static void add_node_to_free_list(struct mm *mm, struct mmnode *node)
{
    assert(mm != NULL && node != NULL);

    node->free_prev = mm->free_last;
    if (mm->free_last) {
        mm->free_last->free_next = node;
    } else {
        mm->free_head = node;
    }

    mm->free_last = node;
    node->free_next = NULL;
}

/**
 * \brief Remove a node from the list of free nodes. This is necessary
 * for cases like coalesce, since two nodes to coalesce might not be
 * adjacent in the free_list.
 *
 * \param mm Pointer to MM allocator instance data.
 * \param node Node to remove from the list of free nodes.
 */
static void remove_node_from_free_list(struct mm *mm, struct mmnode *node)
{
    assert(mm != NULL && node != NULL);

    if (node == mm->free_head) {
        mm->free_head = node->free_next;
        if (mm->free_head) {
            mm->free_head->free_prev = NULL;
        }
    } else {
        node->free_prev->free_next = node->free_next;
    }

    if (node == mm->free_last) {
        mm->free_last = node->free_prev;
        if (mm->free_last) {
            mm->free_last->free_next = NULL;
        }
    } else {
        node->free_next->free_prev = node->free_prev;
    }

    node->free_next = NULL;
    node->free_prev = NULL;
}

/**
 * DONE: also insert to free list
 * \brief simply insert a new node at the front of our linked list structure.
 * If the provided node is of type "NodeType_Free", it is also placed at the
 * head of the free list.
 *
 * \param mm Pointer to MM allocator instance data.
 * \param node Node to remove from the list of free nodes.
 */
static void insert_node_as_head(struct mm *mm, struct mmnode *node)
{
    assert(mm != NULL && node != NULL);

    struct mmnode *old_head = mm->head;

    if (old_head != NULL) {
        old_head->prev = node;
    }
    mm->head = node;
    node->next = old_head;
    node->prev = NULL;

    // maybe also insert into free list
    if (node->type == NodeType_Free) {
        add_node_to_free_list(mm, node);
    } else {
        node->free_next = NULL;
        node->free_prev = NULL;
    }
}

/**
 * TODO: Also, maybe param "a" could be removed, as it will just point
 * to "node" anyways.
 * \brief Split the provided mmnode to create one node with the
 * requested size and one with the remaining size.
 *
 * \param mm Pointer to MM allocator instance data.
 * \param node Existing mmnode that will be split.
 * \param offset Requested size of the newly created node "a".
 * \param a Location where a pointer to a newly created node will
 * be stored. That node will be of size "offset".
 * \param b Location where a pointer to another newly created node
 * will be stored.
 */
static errval_t split_node(struct mm *mm, struct mmnode *node, size_t offset, struct mmnode **a, struct mmnode **b)
{
    assert(mm != NULL && node != NULL);

    struct mmnode *new_node = slab_alloc(&mm->slabs);
    NULLPTR_CHECK(new_node, LIB_ERR_SLAB_ALLOC_FAIL);

    *new_node = *node;
    new_node->base += offset;
    new_node->size -= offset;

    node->size = offset;

    new_node->prev = node;
    node->next = new_node;

    if (new_node->next != NULL) {
        new_node->next->prev = new_node;
    }

    if (new_node->type == NodeType_Free) {
        add_node_to_free_list(mm, new_node);
    }

    *a = node;
    *b = new_node;

    return SYS_ERR_OK;
}

/**
 * QUESTION: should the check for refilling be closer to the function call?
 * \brief Check if the slab allocator requires a refill, and refill it
 * if necessary.
 *
 * \param mm Pointer to MM allocator instance data.
 */
static void mm_check_refill(struct mm *mm)
{
    assert(mm != NULL);

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

/**
 * DONE: free list?
 * \brief Adds an mmnode to the given MM allocator instance data
 *
 * \param mm Pointer to MM allocator instance data
 * \param cap Capability to the given RAM space
 * \param base Start_add of the RAM to allocate
 * \param size Amount of RAM to allocate, in bytes
 */
errval_t mm_add(struct mm *mm, struct capref cap, genpaddr_t base, size_t size)
{
    assert(mm != NULL);

    thread_mutex_lock_nested(&mm->mutex);
    struct mmnode *new_node = slab_alloc(&mm->slabs);

    if (new_node == NULL) {
        thread_mutex_unlock(&mm->mutex);
        return LIB_ERR_SLAB_ALLOC_FAIL;
    }

    new_node->cap = (struct capinfo) {
        .cap = cap,
        .base = base,
        .size = size
    };

    new_node->type = NodeType_Free;

    insert_node_as_head(mm, new_node);

    new_node->base = base;
    new_node->size = size;
    mm->stats_bytes_available += size;
    mm->stats_bytes_max += size;

    thread_mutex_unlock(&mm->mutex);
    return SYS_ERR_OK;
}

/**
 * DONE: loop free list
 * \brief Allocates aligned memory in the form of a RAM capability
 *
 * \param mm Pointer to MM allocator instance data
 * \param size Amount of RAM to allocate, in bytes
 * \param alignment Alignment of RAM to allocate slot used for the cap in #ret, if any
 * \param retcap Pointer to capref struct, filled-in with allocated cap location
 */
errval_t mm_alloc_aligned(struct mm *mm, size_t size, size_t alignment, struct capref *retcap)
{
    assert(mm != NULL);

    if (alignment == 0) {
        DEBUG_PRINTF("invalid alignment of 0, using 1 instead");
        alignment = 1;
    }
    size = ROUND_UP(size, BASE_PAGE_SIZE);

    errval_t err;

    err = mm_slot_alloc(mm, retcap);
    ON_ERR_PUSH_RETURN(err, LIB_ERR_SLOT_ALLOC);

    struct mmnode *node;


    thread_mutex_lock_nested(&mm->mutex);

    for (node = mm->free_head; node != NULL; node = node->free_next) {
        if (node->size >= size) {
            struct mmnode *a, *b;

            if (node->base % alignment != 0) {
                uint64_t offset = alignment - (node->base % alignment);
                if (node->size - offset < size) {
                    continue;
                }
                err = split_node(mm, node, offset, &a, &b);
                if (err_is_fail(err)) {
                    thread_mutex_unlock(&mm->mutex);
                    return err;
                }

                node = b;
            }

            err = cap_retype(*retcap, node->cap.cap, node->base - node->cap.base, mm->objtype, size, 1);
            if (err_is_fail(err)) {
                thread_mutex_unlock(&mm->mutex);
                return err_push(err, LIB_ERR_CAP_RETYPE);
            }

            if (size < node->size) {
                err = split_node(mm, node, size, &a, &b);
                if (err_is_fail(err)) {
                    thread_mutex_unlock(&mm->mutex);
                    return err;
                }

                node = a;
            }

            node->type = NodeType_Allocated;
            remove_node_from_free_list(mm, node);
            mm->stats_bytes_available -= node->size;

            mm_check_refill(mm);
            thread_mutex_unlock(&mm->mutex);
            return SYS_ERR_OK;
        }
    }


    thread_mutex_unlock(&mm->mutex);
    return LIB_ERR_RAM_ALLOC_FIXED_EXHAUSTED;
}

errval_t mm_alloc(struct mm *mm, size_t size, struct capref *retcap)
{
    assert(mm != NULL);

    return mm_alloc_aligned(mm, size, BASE_PAGE_SIZE, retcap);
}


/**
 * DONE: update free list
 * \brief Tries to merge an mmnode with its right neighbour.
 *        The merging only happens if they are adjacent and both free.
 *
 * \param mm Pointer to MM allocator instance data
 * \param mmnode Pointer to the mmnode
 */
static bool coalesce(struct mm *mm, struct mmnode *node)
{
    assert(mm != NULL);

    if (node == NULL || node->next == NULL || !capcmp(node->next->cap.cap, node->cap.cap)) {
        return false;
    }

    struct mmnode *right = node->next;

    if (node->type == NodeType_Free && right->type == NodeType_Free) {
        assert(node->base + node->size == right->base);

        node->size += right->size;
        node->next = right->next;

        if (node->next != NULL) {
            node->next->prev = node;
        }

        remove_node_from_free_list(mm, right); // right node no longer usable
        slab_free(&mm->slabs, right);

        return true;
    }
    return false;
}

/**
 * DONE: add to free list
 * \brief Freeing allocated RAM and associated capability
 *
 * \param mm Pointer to MM allocator instance data
 * \param cap The capability fot the allocated RAM
 * \param base The start_addr for allocated RAM the cap is for
 * \param size The size of the allocated RAM the capability is for
 */
errval_t mm_free(struct mm *mm, struct capref cap, genpaddr_t base, gensize_t size)
{
    assert(mm != NULL);

    errval_t err;


    thread_mutex_lock_nested(&mm->mutex);

    for (struct mmnode* node = mm->head; node; node = node->next) {
        if (node->base == base && node->size == size) {
            node->type = NodeType_Free;
            err = cap_destroy(cap);

            // Spannend
            if (err_no(err) == LIB_ERR_WHILE_FREEING_SLOT) {
                err = err_pop(err);
                if (err_no(err) == LIB_ERR_SLOT_ALLOC_WRONG_CNODE) {
                    err = mm_slot_free(mm, cap);
                }
            }
            if(err_is_fail(err)) {
                thread_mutex_unlock(&mm->mutex);
                return err_push(err, LIB_ERR_CAP_DESTROY);
            }


            mm->stats_bytes_available += size;

            coalesce(mm, node);

            // if node can be coalesced with its predecessor, that one is added to
            // the free list, otherwise just add node
            struct mmnode *new_free = node->prev;
            new_free = coalesce(mm, new_free) ? new_free : node;

            add_node_to_free_list(mm, new_free);

            return SYS_ERR_OK;
        }
    }

    thread_mutex_unlock(&mm->mutex);
    return LIB_ERR_RAM_ALLOC_WRONG_SIZE;
}


errval_t mm_slot_free(struct mm *mm, struct capref cap)
{
    assert(mm != NULL);

    return slot_prealloc_free((struct slot_prealloc*) mm->slot_alloc_inst, cap);
}


/**
 * \brief Print the state of an mm-instance: how many free nodes exist, how
 * many unfree ones etc.
 *
 * \param mm Pointer to MM allocator instance data
 */
void print_mm_state(struct mm *mm)
{
    size_t free_nodes = 0;
    size_t unfree_nodes = 0;
    for (struct mmnode* node = mm->head; node != NULL; node = node->next) {
        if (node->type == NodeType_Free) {
            printf("free   ");
            free_nodes++;
        }
        else {
            printf("unfree ");
            unfree_nodes++;
        }
        printf("mmnode with base 0x%x size 0x%x\n", node->base, node->size);
    }
    printf("free nodes: %d, unfree nodes: %d\n", free_nodes, unfree_nodes);

    int free_count = 0;
    for (struct mmnode* node = mm->free_head; node; node = node->free_next)
        free_count++;
    printf("%d nodes in free list\n", free_count);
}
