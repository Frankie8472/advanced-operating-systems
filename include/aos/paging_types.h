/**
 * \file
 * \brief PMAP Implementaiton for AOS
 */

/*
 * Copyright (c) 2019 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetstr. 6, CH-8092 Zurich. Attn: Systems Group.
 */

#ifndef PAGING_TYPES_H_
#define PAGING_TYPES_H_ 1

#include <aos/solution.h>
#include <arch/aarch64/barrelfish_kpi/paging_arch.h>
#include <aos/slab.h>
#include <string.h>

#define VADDR_OFFSET ((lvaddr_t)512UL*1024*1024*1024) // 1GB
#define VREGION_FLAGS_READ     0x01 // Reading allowed
#define VREGION_FLAGS_WRITE    0x02 // Writing allowed
#define VREGION_FLAGS_EXECUTE  0x04 // Execute allowed
#define VREGION_FLAGS_NOCACHE  0x08 // Caching disabled
#define VREGION_FLAGS_MPB      0x10 // Message passing buffer
#define VREGION_FLAGS_GUARD    0x20 // Guard page
#define VREGION_FLAGS_MASK     0x2f // Mask of all individual VREGION_FLAGS

#define VREGION_FLAGS_READ_WRITE \
    (VREGION_FLAGS_READ | VREGION_FLAGS_WRITE)
#define VREGION_FLAGS_READ_EXECUTE \
    (VREGION_FLAGS_READ | VREGION_FLAGS_EXECUTE)
#define VREGION_FLAGS_READ_WRITE_NOCACHE \
    (VREGION_FLAGS_READ | VREGION_FLAGS_WRITE | VREGION_FLAGS_NOCACHE)
#define VREGION_FLAGS_READ_WRITE_MPB \
    (VREGION_FLAGS_READ | VREGION_FLAGS_WRITE | VREGION_FLAGS_MPB)

typedef int paging_flags_t;

enum paging_region_type {
    PAGING_REGION_FREE, ///< for free paging regions (may be allocated)
    PAGING_REGION_UNUSABLE, ///< for occupied regions (addresses less than VADDR_OFFSET)
    PAGING_REGION_HEAP, ///< for malloc area e.a.
    PAGING_REGION_STACK, ///< for thread stacks
    PAGING_REGION_OTHER,
};


struct paging_region {
    lvaddr_t base_addr;
    lvaddr_t current_addr;
    size_t region_size;

    char region_name[32]; //< for debugging purposes

    enum paging_region_type type;
    bool lazily_mapped;
    bool map_large_pages;
    paging_flags_t flags; ///< lazily mapped pages should be mapped using this flag
    
    struct paging_region *next;
    struct paging_region *prev;
    // TODO: if needed add struct members for tracking state
};

struct stack_guard {
    struct stack_guard* next;
    lvaddr_t stack_bottom;
    uintptr_t thread_id;
};


/**
 * \brief shadow page table
 */
struct mapping_table
{
    /// capref to the pagetable
    struct capref pt_cap;

    /// paging region that contains the first vaddr that is mapped in this pt
    struct paging_region *region;

    /// mappings in this table
    struct capref mapping_caps[PTABLE_ENTRIES];

    /// \brief child tables
    /// 
    /// \note If at a certain index this array is NULL but the corresponding
    ///       entry in mapping_caps is not a null capref, this means that a
    ///       superpage mapping is done in this entry
    ///
    struct mapping_table *children[PTABLE_ENTRIES];
};


// struct to store the paging status of a process
struct paging_state {
    struct thread_mutex mutex;
    struct slot_allocator *slot_alloc;      // Slot allocator

    struct mapping_table map_l0;            // Shadow page table lv 0
    struct slab_allocator mappings_alloc;   // Slab allocator for shadow page table
    bool mappings_alloc_is_refilling;       // Boolean for blocking while refilling

    struct paging_region *head;             // Head of the vaddr region linked list

    struct paging_region vaddr_offset_region;   // Region from 0x0 to VADDR_OFFSET

    struct paging_region free_region;   // Free Region at the end

    struct paging_region heap_region;   // Heap region
    struct paging_region meta_region;   // Meta region
    struct paging_region stack_region;  // Stack region
};


__attribute__((unused)) static void init_mapping_table(struct mapping_table *mt)
{
    memset(mt, 0, sizeof(struct mapping_table));
}

#endif  /// PAGING_TYPES_H_
