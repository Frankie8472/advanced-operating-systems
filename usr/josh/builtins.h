#ifndef JOSH_BUILTINS_H
#define JOSH_BUILTINS_H

#include "ast.h"
#include <aos/aos_datachan.h>

int is_builtin(const char* cmd);

int run_builtin(const char *cmd, size_t argc, const char **argv, struct aos_datachan *out);

#endif // JOSH_BUILTINS_H
