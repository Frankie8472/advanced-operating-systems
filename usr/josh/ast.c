#include "ast.h"


void josh_line_free(struct josh_line *line)
{
    if (line->cmd) {
        free(line->cmd);
        line->cmd = NULL;
    }
    if (line->destination) {
        free(line->destination);
        line->destination = NULL;
    }
    array_list_free(&line->args);
    free(line);
}

