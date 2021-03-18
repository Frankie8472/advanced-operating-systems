/**
 * \file
 * \brief A library for managing physical memory (i.e., caps)
 */

#include <mm/mm.h>
#include <aos/debug.h>
#include <aos/solution.h>


const size_t SLAB_REFILL_THRESHOLD = 6;



errval_t mm_init(struct mm *mm, enum objtype objtype,
                     slab_refill_func_t slab_refill_func,
                     slot_alloc_t slot_alloc_func,
                     slot_refill_t slot_refill_func,
                     void *slot_alloc_inst)
{
    slab_init(&mm->slabs, sizeof(struct mmnode), slab_refill_func);
    mm->head = NULL;
    mm->objtype = objtype;
    mm->slot_alloc = slot_alloc_func;
    mm->slot_refill = slot_refill_func;
    mm->slot_alloc_inst = slot_alloc_inst;
    mm->refilling = false;
    return SYS_ERR_OK;
}

void mm_destroy(struct mm *mm)
{
    assert(!"NYI");
}

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

static void mm_check_refill(struct mm *mm)
{
    if (!mm->refilling) {
        int freec = slab_freecount(&mm->slabs);
        //printf("free slabs: %d\n", freec);
        if (freec <= SLAB_REFILL_THRESHOLD) {
            mm->refilling = true;
            //printf("refilling slab allocator\n");
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

    //DEBUG_PRINTF("memory region added!\n");
    return SYS_ERR_OK;
}


errval_t mm_alloc_aligned(struct mm *mm, size_t size, size_t alignment, struct capref *retcap)
{
    if (alignment == 0) {
        DEBUG_PRINTF("invalid alignment of 0, using 1 instead");
        alignment = 1;
    }

    mm->slot_alloc(mm->slot_alloc_inst, 1, retcap);

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
            goto ok_refill;
        }
        node = node->next;
    }

    //printf("could not allocate a frame!\n");
    return LIB_ERR_RAM_ALLOC_FIXED_EXHAUSTED;

ok_refill:
    mm_check_refill(mm);
    return SYS_ERR_OK;
}

errval_t mm_alloc(struct mm *mm, size_t size, struct capref *retcap)
{
    return mm_alloc_aligned(mm, size, BASE_PAGE_SIZE, retcap);
}


/**
 * \brief tries to merge an mmnode with its right neighbour
 * 
 * The merging only happens if they are adjacent and both free
 * 
 * \return <code>true</code> if any merging was done
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
