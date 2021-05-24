#ifndef JOSH_BUILTINS_H
#define JOSH_BUILTINS_H

#include "ast.h"

int is_builtin(const char* cmd);

int run_builtin(const char *cmd, size_t argc, const char **argv);

#endif // JOSH_BUILTINS_H
