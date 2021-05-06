
#include <arch/arm/ipi.h>
#include <arch/arm/gic.h>
#include <arch/aarch64/global.h>


bool platform_is_ipi_notify_interrupt(int irq)
{
    return irq == IPI_NOTIFY_IRQ;
}


void platform_send_ipi_notify(coreid_t core_id)
{
    gic_raise_softirq(core_id, IPI_NOTIFY_IRQ);
}


genpaddr_t *get_global_notify_ptrs(void)
{
    return global->notify;
}

