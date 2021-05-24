#ifndef JOSH_BUILTINS_H
#define JOSH_BUILTINS_H

#include "ast.h"

int is_builtin(const char* cmd);

int run_builtin(struct josh_command *line);

#endif // JOSH_BUILTINS_H
