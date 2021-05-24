#include "ast.h"


void josh_line_free(struct josh_line *line)
{
    if (line->type == JL_COMMAND) {
        josh_command_free(line->command);
        line->command = NULL;
    }
    else if (line->type == JL_ASSIGNMENT) {
        josh_assignment_free(line->assignment);
        line->assignment = NULL;
    }
    free(line);
}


void josh_value_free(struct josh_value *val)
{
    if (val->val != NULL) {
        free(val->val);
    }
    free(val);
}


void josh_assignment_free(struct josh_assignment *assgn)
{
    if (assgn->varname != NULL) {
        free(assgn->varname);
        assgn->varname = NULL;
    }
    if (assgn->value != NULL) {
        josh_value_free(assgn->value);
        assgn->value = NULL;
    }
    free(assgn);
}


void josh_command_free(struct josh_command *cmd)
{
    if (cmd->cmd) {
        free(cmd->cmd);
        cmd->cmd = NULL;
    }
    if (cmd->destination) {
        free(cmd->destination);
        cmd->destination = NULL;
    }
    for (size_t i = 0; i < cmd->args.length; i++) {
        struct josh_value *arg = *(struct josh_value **) array_list_at(&cmd->args, i);
        free(arg);
    }
    array_list_free(&cmd->args);

    free(cmd);
}

