#include "spawn/process_manager.h"



struct process_manager
{
    struct slab_allocator si_allocator;
    struct spawninfo *first;
} instance;


/**
 * \brief slab refill function that refills big chunks at once
 */
static errval_t slab_big_refill(struct slab_allocator *slabs)
{
    const size_t bytes = BASE_PAGE_SIZE * 16;
    struct capref frame;
    errval_t err;
    err = frame_alloc(&frame, bytes, NULL);
    ON_ERR_PUSH_RETURN(err, LIB_ERR_FRAME_ALLOC);
    
    void *buf;
    err = paging_map_frame_complete(get_current_paging_state(), &buf, frame, NULL, NULL);
    ON_ERR_PUSH_RETURN(err, LIB_ERR_PMAP_DO_MAP);

    slab_grow(slabs, buf, bytes);
    return SYS_ERR_OK;
}

static struct process_manager *get_process_manager(void)
{
    static bool initialized = false;
    if (!initialized) {
        slab_init(&instance.si_allocator, sizeof(struct spawninfo), &slab_big_refill);
        slab_big_refill(&instance.si_allocator);
        instance.first = NULL;
        initialized = true;
    }
    return &instance;
}


struct spawninfo *spawn_create_spawninfo(void)
{
    struct process_manager *pm = get_process_manager();
    struct spawninfo *si = slab_alloc(&pm->si_allocator);
    //memset(si, 0, sizeof(struct spawninfo));
    if (si == NULL) {
        return NULL;
    }

    // insert new si at head of list
    si->next = pm->first;
    pm->first = si;

    return si;
}


struct aos_rpc *get_rpc_from_spawn_info(domainid_t pid){
    struct process_manager *pm = get_process_manager();
    for(struct spawninfo * curr = pm -> first; curr != NULL;curr = curr -> next ){
        if(curr  -> pid == pid){

            return &curr -> rpc;
        }
    }

    assert(false && "Did not find rpc from spawninfos for pid");
    return NULL;
}


struct spawninfo *get_si_from_rpc(struct aos_rpc * rpc){
    struct process_manager *pm = get_process_manager();
    for(struct spawninfo * curr = pm -> first; curr != NULL;curr = curr -> next ){
        if(&curr  -> rpc == rpc){

            return curr;
        }
    }
    assert(false && "Did not find si from rpc");
    return NULL;
}

// domainid_t spawn_get_new_domainid(void)
// {
//     struct process_manager *pm = get_process_manager();
//     debug_printf("created pid: %d\n", pm->next_pid);
//     return pm->next_pid++ + (disp_get_current_core_id() << 10);
// }
