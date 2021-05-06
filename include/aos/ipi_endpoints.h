#ifndef LIBAOS_IPI_ENDPOINTS_H
#define LIBAOS_IPI_ENDPOINTS_H

#include <sys/cdefs.h>

#include <aos/waitset.h>

__BEGIN_DECLS

/// LMP endpoint structure (including data accessed only by user code)
struct ipi_endpoint {
    struct ipi_endpoint *next, *prev; ///< Next/prev endpoint in list
    struct waitset_chanstate waitset_state; ///< Waitset per-channel state
};


__END_DECLS

#endif // LIBAOS_IPI_ENDPOINTS_H
