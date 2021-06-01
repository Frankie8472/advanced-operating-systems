#ifndef JOSH_MAIN_H
#define JOSH_MAIN_H

#include <aos/aos.h>
#include "running.h"

errval_t spawn_program(const char *dest, const char *cmd, size_t argc, const char **argv, struct running_program *prog);


#endif // JOSH_MAIN_H
