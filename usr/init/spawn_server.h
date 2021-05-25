#ifndef INIT_SPAWN_SERVER_H_
#define INIT_SPAWN_SERVER_H_


#include <aos/aos.h>
#include <spawn/spawn.h>

errval_t spawn_new_core(coreid_t core);
errval_t spawn_new_domain(const char *mod_name, int argc, char **argv, domainid_t *new_pid,
                          struct capref spawner_ep_cap, struct capref stdout_ep, struct spawninfo **ret_si);

errval_t spawn_lpuart_driver(const char *mod_name, struct spawninfo **ret_si);
errval_t spawn_enet_driver(const char *mod_name, struct spawninfo **ret_si);

#endif // INIT_SPAWN_SERVER_H_
