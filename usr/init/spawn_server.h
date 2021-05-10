#ifndef INIT_SPAWN_SERVER_H_
#define INIT_SPAWN_SERVER_H_


#include <aos/aos.h>

errval_t spawn_new_core(coreid_t core);
errval_t spawn_new_domain(const char *mod_name, domainid_t *new_pid);

errval_t spawn_lpuart_driver(const char *mod_name);

#endif // INIT_SPAWN_SERVER_H_
