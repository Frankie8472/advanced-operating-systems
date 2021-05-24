#ifndef JOSH_AST_H
#define JOSH_AST_H

#include <collections/array_list.h>

struct josh_command
{
    char *cmd;
    struct array_list args;
    char *destination;
};


enum josh_valtype
{
    JV_LITERAL,
    JV_VARIABLE,
};

struct josh_value
{
    enum josh_valtype type;
    char *val;
};

void josh_line_free(struct josh_command *line);

#endif // JOSH_AST_H
