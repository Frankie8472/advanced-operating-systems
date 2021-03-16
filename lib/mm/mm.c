/**
 * \file
 * \brief A library for managing physical memory (i.e., caps)
 */

#include <mm/mm.h>
#include <aos/debug.h>
#include <aos/solution.h>



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

    printf("memory region added!\n");
    return SYS_ERR_OK;
}


errval_t mm_alloc_aligned(struct mm *mm, size_t size, size_t alignment, struct capref *retcap)
{
    mm->slot_alloc(mm->slot_alloc_inst, 1, retcap);

    struct mmnode *node = mm->head;
    size_t size_rounded_up = size;//(size & 0xFFF) ? ((size & ~0xFFF) + 0x1000) : size;

    while(node != NULL) {
        if (node->type == NodeType_Free && node->size >= size) {
            struct mmnode *a, *b;

            if (node->base % alignment != 0) {
                uint64_t offset = alignment - (node->base % alignment);
                if (node->size - offset < size) {
                    node = node->next;
                    continue;
                }
                errval_t err = split_node(mm, node, offset, &a, &b);
                if (err_is_fail(err)) {
                    DEBUG_ERR(err, "could not align memory");
                    return err;
                }
                node = b;
            }

            errval_t err = cap_retype(*retcap,
                node->cap.cap,
                node->base - node->cap.base,
                mm->objtype,
                size_rounded_up,
                1
            );

            if (err_is_fail(err)) {
                DEBUG_ERR(err, "could not retype region cap");
                return err;
            }

            err = split_node(mm, node, size_rounded_up, &a, &b);
            if (err_is_fail(err)) {
                DEBUG_ERR(err, "error splitting mmnodes");
                return err;
            }

            a->type = NodeType_Allocated;
            goto ok_refill;
        }
        node = node->next;
    }

    //printf("could not allocate a frame!\n");
    return LIB_ERR_RAM_ALLOC_FIXED_EXHAUSTED;


    static volatile bool refilling = false;
ok_refill:
    if (!refilling) {
        int freec = slab_freecount(&mm->slabs);
        //printf("free slabs: %d\n", freec);
        if (freec <= 4) {
            refilling = true;
            //printf("refilling slab allocator\n");
            slab_default_refill(&mm->slabs);
            refilling = false;
        }
    }
    return SYS_ERR_OK;
}

errval_t mm_alloc(struct mm *mm, size_t size, struct capref *retcap)
{
    return mm_alloc_aligned(mm, size, BASE_PAGE_SIZE, retcap);
}


/**
 * \brief merges two free adjacent mmnodes and returns the merged node
 * 
 * \return the merged node or <code>NULL</code> if no merging was done
 */
static struct mmnode* merge(struct mm *mm, struct mmnode *node) {
    struct mmnode *left = node->prev;
    if (left != NULL) {
        if (capcmp(left->cap.cap, node->cap.cap)
            && node->type == NodeType_Free
            && left->type == NodeType_Free) {
            assert(left->base + left->size == node->base);
            left->size += node->size;
            left->next = node->next;
            if (left->next != NULL) {
                left->next->prev = left;
            }
            slab_free(&mm->slabs, node);
            return left;
        }
    }

    struct mmnode *right = node->next;
    if (right != NULL) {
        if (capcmp(right->cap.cap, node->cap.cap)
            && node->type == NodeType_Free
            && right->type == NodeType_Free) {
            assert(node->base + node->size == right->base);
            node->size += right->size;
            node->next = right->next;
            if (node->next != NULL) {
                node->next->prev = left;
            }
            slab_free(&mm->slabs, right);
            return node;
        }
    }

    return NULL;
}


errval_t mm_free(struct mm *mm, struct capref cap, genpaddr_t base, gensize_t size)
{
    struct mmnode* node = mm->head;
    while(node != NULL) {
        if (node->base == base && node->size == size) {
            node->type = NodeType_Free;
            /*errval_t err = cap_revoke(cap);
            if (err_is_fail(err)) {
                DEBUG_ERR(err, "could not revoke ram cap");
                return err;
            }*/

            errval_t err = cap_destroy(cap);
            if (err_is_fail(err)) {
                DEBUG_ERR(err, "could not destroy ram cap");
                return err;
            }

            // merge until no mergeing can be done anymore
            while((node = merge(mm, node)));
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
