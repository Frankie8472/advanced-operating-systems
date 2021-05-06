#ifndef ARCH_ARM_IPI_H
#define ARCH_ARM_IPI_H

#define IPI_NOTIFY_IRQ 12

#include <kernel.h>

bool platform_is_ipi_notify_interrupt(int irq);
void platform_send_ipi_notify(coreid_t core_id);
genpaddr_t *get_global_notify_ptrs(void);

#endif // ARCH_ARM_IPI_H
