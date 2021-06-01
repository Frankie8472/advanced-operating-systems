#ifndef JOSH_BUILTINS_H
#define JOSH_BUILTINS_H

#include "ast.h"
#include <aos/aos_datachan.h>

int is_builtin(const char* cmd);

int run_builtin(const char *cmd, size_t argc, const char **argv, struct aos_datachan *out);
bool check_for_option(size_t argc,const char **argv, const char* option);
bool find_query(size_t argc, const char ** argv,  char* query);
bool find_property(size_t argc, const char ** argv,  char* properties);
bool is_flag(const char* string);
#endif // JOSH_BUILTINS_H
