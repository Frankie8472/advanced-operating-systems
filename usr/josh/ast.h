#ifndef JOSH_AST_H
#define JOSH_AST_H

#include <collections/array_list.h>

struct josh_line
{
    char *cmd;
    struct array_list args;
};


enum josh_valtype
{
    JV_STRING,
    JV_ENVVAR,
};

struct josh_value
{

};

void josh_line_free(struct josh_line *line);

#endif // JOSH_AST_H
