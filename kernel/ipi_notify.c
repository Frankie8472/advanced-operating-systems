/**
 * \file
 * \brief Inter-processor interrupt (IPI) notify mechanism.
 */

/*
 * Copyright (c) 2007, 2008, 2010, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetstrasse 6, CH-8092 Zurich. Attn: Systems Group.
 */

#include <kernel.h>
#include <ipi_notify.h>
#include <arch/arm/ipi.h>

#include <dispatch.h>
#include <capabilities.h>
#include <barrelfish_kpi/syscalls.h>
#include <barrelfish_kpi/paging_arch.h>


// I took this file from the official barrelfish repo (https://github.com/BarrelfishOS/barrelfish)
// It was originally located under kernel/arch/x86/ipi_notify.c, but as it barely contains
// too architecture-specific code I moved it to kernel/ipi_notify.c

// TODO: These arrays in this file use a lot of memory... Maybe try to make them a little more compact


// Max number of notification IDs that fit into message passing buffers
#define MAX_CHANIDS             65535

/// User-space endpoints awaiting notifications
static struct cte endpoints[MAX_CHANIDS];

#define NOTIFY_FIFO_SIZE        64
#define NOTIFY_FIFO_BYTES       (NOTIFY_FIFO_SIZE * sizeof(uint64_t))

/// Page used for notify FIFOs
static char my_notify_page[NOTIFY_FIFO_BYTES * MAX_COREID];

// Private head/tail pointers for notify FIFOs
static uint64_t notifyhead[MAX_COREID]; 
static uint64_t notifytail[MAX_COREID];

static uint8_t my_arch_id;

errval_t ipi_register_notification(capaddr_t ep, int chanid)
{
    struct cte *recv;
    errval_t err;

    err = caps_lookup_slot(&dcb_current->cspace.cap, ep,
                           2, &recv, CAPRIGHTS_WRITE);

    if (err_is_fail(err)) {
        return err_push(err, SYS_ERR_IRQ_LOOKUP);
    }

    return ipi_register_notification_cte(recv, chanid);
}


errval_t ipi_register_notification_cte(struct cte *recv, int chanid)
{
    assert(recv != NULL);

    // Return w/error if cap is not an endpoint
    if(recv->cap.type != ObjType_EndPointLMP) {
        return SYS_ERR_IRQ_NOT_ENDPOINT;
    }

    // Return w/error if no listener on endpoint
    if(recv->cap.u.endpointlmp.listener == NULL) {
        return SYS_ERR_IRQ_NO_LISTENER;
    }

    if(chanid < MAX_CHANIDS) {
        // check that we don't overwrite someone else's handler
        if (endpoints[chanid].cap.type != ObjType_Null) {
            printf("kernel: installing new handler for IPI notification %d\n", chanid);
        }
        return caps_copy_to_cte(&endpoints[chanid], recv, false, 0, 0);
    } else {
        return SYS_ERR_IRQ_INVALID;
    }
}


void ipi_handle_notify(void)
{
    uint64_t val = 0;

    genpaddr_t *notify = get_global_notify_ptrs();

    for(coreid_t srccore = 0; srccore < MAX_COREID; srccore++) {
        volatile uint64_t *fifo = (void *)&my_notify_page[NOTIFY_FIFO_BYTES * srccore];

        if (notify[my_arch_id] == 0) {
            panic("NO PCN for core %d!", my_arch_id);
        }

        // Which slot in the fifo to poll
        uint64_t slot = notifytail[srccore] % NOTIFY_FIFO_SIZE;

        while (fifo[slot] != 0) {
            val = fifo[slot];

            if (endpoints[val].cap.type == ObjType_Null) {
                printk(LOG_ERR, "Received IPI Notify interrupt for channel %d but no handler set!\n", val);
            }
            else {
                lmp_deliver_notification(&endpoints[val].cap);
            }

            fifo[slot] = 0; // ACK
            notifytail[srccore]++;
            slot = notifytail[srccore] % NOTIFY_FIFO_SIZE;
        }
    }

    // XXX: Dispatch last listener -- should be done by scheduler
    if(val != 0) {
        dispatch(endpoints[val].cap.u.endpointlmp.listener);
    }

    printk(LOG_ERR, "Received IPI Notify interrupt for channel %d but no handler set!\n", val);
}

struct sysret ipi_raise_notify(coreid_t coreid, uintptr_t chanid)
{
    genpaddr_t *notify = get_global_notify_ptrs();
    char *notify_page = (char *)local_phys_to_mem(notify[coreid]);

    if (notify_page == NULL || coreid >= MAX_COREID) {
        printf("UMPNOTIFY ERROR!\n");
        return SYSRET(SYS_ERR_ILLEGAL_INVOCATION);
    }

    // Locate our private notification fifo and head ptr
    volatile uint64_t *fifo = (void *)&notify_page[my_arch_id * NOTIFY_FIFO_BYTES]; 
    uint64_t slot = notifyhead[coreid] % NOTIFY_FIFO_SIZE;

    // Make sure the next slot is empty
    if (fifo[slot] != 0) {
        panic("FULL");
    }

    // Update notify fifo
    fifo[slot] = (uint64_t)chanid;
    notifyhead[coreid]++;

    // Send IPI to dest kernel
    //apic_send_std_ipi(coreid, xapic_none, APIC_INTER_CORE_VECTOR);
    platform_send_ipi_notify(coreid);

    return SYSRET(SYS_ERR_OK);
}


void ipi_notify_init(void)
{
    my_arch_id = my_core_id;

    // Publish the address of the notify page in the global kernel state
    genpaddr_t *notify = get_global_notify_ptrs();
    notify[my_arch_id] = local_phys_to_gen_phys(mem_to_local_phys((lvaddr_t)my_notify_page));
}


uint32_t ipi_alloc_channel(void)
{
    static uint32_t counter = 1;

    // TODO: implement useful channel management
    return counter++;
}
