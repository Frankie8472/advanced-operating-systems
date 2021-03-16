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

void insert_node_as_head(struct mm *mm, struct mmnode *node);
void insert_node_as_head(struct mm *mm, struct mmnode *node)
{
    struct mmnode *old_head = mm->head;

    if (old_head != NULL) {
        old_head->prev = node;
    }
    mm->head = node;
    node->next = old_head;
    node->prev = NULL;
}

errval_t mm_add(struct mm *mm, struct capref cap, genpaddr_t base, size_t size)
{
    struct mmnode *new_node = slab_alloc(&mm->slabs);

    insert_node_as_head(mm, new_node);

    new_node->type = NodeType_Free;
    new_node->cap.cap = cap;
    new_node->cap.base = base;
    new_node->cap.size = size;
    new_node->base = base;
    new_node->size = size;

    printf("memory region added!\n");
    return SYS_ERR_OK;
}


errval_t mm_alloc_aligned(struct mm *mm, size_t size, size_t alignment, struct capref *retcap)
{
    printf("calling mm_alloc_aligned!\n");

    struct mmnode* node = mm->head;

    size_t size_rounded_up = size;//(size & 0xFFF) ? ((size & ~0xFFF) + 0x1000) : size;

    while(node != NULL) {
        if (node->type == NodeType_Free && node->size >= size) {
            struct capref slots[1];
            mm->slot_alloc(mm->slot_alloc_inst, 1, slots);

            errval_t err = cap_retype(slots[0],
                node->cap.cap,
                node->base - node->cap.base,
                mm->objtype,
                size_rounded_up,
                1
            );

            if (err_is_fail(err)) {
                DEBUG_ERR(err, "could not retype");
                return err;
            }

            struct mmnode* new_node = slab_alloc(&mm->slabs);
            if (new_node == NULL) {
                errval_t errv;
                err_push(errv, LIB_ERR_SLAB_ALLOC_FAIL);
                return errv;
            }
            new_node->base = node->base;
            new_node->size = size_rounded_up;
            new_node->cap = node->cap;
            new_node->type = NodeType_Allocated;
            insert_node_as_head(mm, new_node);

            node->base += size_rounded_up;

            *retcap = slots[0];
            printf("allocated a frame!\n");
            return SYS_ERR_OK;
        }
        node = node->next;
    }

    printf("could not allocate a frame!\n");
    return LIB_ERR_NOT_IMPLEMENTED;
}

errval_t mm_alloc(struct mm *mm, size_t size, struct capref *retcap)
{
    return mm_alloc_aligned(mm, size, BASE_PAGE_SIZE, retcap);
}


errval_t mm_free(struct mm *mm, struct capref cap, genpaddr_t base, gensize_t size)
{
    struct mmnode* node = mm->head;
    while(node != NULL) {
        if (node->base == base && node->size == size) {
            node->type = NodeType_Free;
            return cap_destroy(cap);
        }
    }
    return LIB_ERR_CAP_COPY_FAIL;

}
