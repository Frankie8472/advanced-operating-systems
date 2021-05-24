#ifndef JOSH_AST_H
#define JOSH_AST_H

#include <collections/array_list.h>

struct josh_command;
struct josh_value;
struct josh_assignment;
struct josh_line;

struct josh_command
{
    char *cmd;
    struct array_list args;
    char *destination;
};

enum josh_value_type
{
    JV_LITERAL,
    JV_VARIABLE,
};

struct josh_value
{
    enum josh_value_type type;
    char *val;
};


struct josh_assignment
{
    char *varname;
    struct josh_value *value;
};


enum josh_line_type
{
    JL_ASSIGNMENT,
    JL_COMMAND,
    JL_BLOCK,
};

struct josh_line
{
    enum josh_line_type type;
    union {
        struct josh_assignment *assignment;
        struct josh_command *command;
    };
};

void josh_line_free(struct josh_line *line);
void josh_value_free(struct josh_value *val);
void josh_assignment_free(struct josh_assignment *assgn);
void josh_command_free(struct josh_command *cmd);

#endif // JOSH_AST_H
