#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <devif/queue_interface_backend.h>
#include <devif/backends/net/enet_devif.h>
#include <aos/aos.h>
#include <aos/deferred.h>
#include <driverkit/driverkit.h>
#include <dev/imx8x/enet_dev.h>
#include <netutil/etharp.h>
#include <netutil/htons.h>

#include <collections/hash_table.h>

/* #include "enet_regionman.h" */
/* #include "enet_handler.h" */
#include "enet.h"

/**
 * \brief given an id and a driver state, retrieve the corresponding udp socket struct
 * \return reference to the according udp socket, NULL if none could be found
 */
__unused
static struct aos_udp_socket* get_socket(struct enet_driver_state *st,
                                         uint64_t socket_id) {
    for (struct aos_udp_socket *i; i; i = i->next) {
        if (i->sock_id == socket_id)
            return i;
    }
    return NULL;
}
