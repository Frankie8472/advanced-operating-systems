#include "builtins.h"

struct builtin
{
    const char *cmd;
    int(*handler)(struct josh_line *line);
};

int handle_echo(struct josh_line *line);
int handle_env(struct josh_line *line);
int handle_nslist(struct josh_line *line);

const struct builtin builtins[] = {
    { "echo", &handle_echo },
    { "env", &handle_env },
    { "nslist", &handle_nslist },
};


int is_builtin(const char* cmd)
{
    for (size_t i = 0; i < sizeof(builtins) / sizeof(builtins[0]); i++) {
        if (strcmp(cmd, builtins[i].cmd) == 0) {
            return 1;
        }
    }
    return 0;
}


int run_builtin(struct josh_line *line)
{
    for (size_t i = 0; i < sizeof(builtins) / sizeof(builtins[0]); i++) {
        if (strcmp(line->cmd, builtins[i].cmd) == 0) {
            return builtins[i].handler(line);
        }
    }
    return 1;
}


int handle_echo(struct josh_line *line)
{
    for (size_t i = 0; i < line->args.length; i++) {
        char **arg = array_list_at(&line->args, i);
        printf("%s", *arg);
        if (i < line->args.length - 1) {
            printf(" ");
        }
    }
    printf("\n ");
    return 0;
}

extern char **environ;
int handle_env(struct josh_line *line)
{
    for (char **ev = environ; *ev != NULL; ev++) {
        for (char *var = *ev; *var != '\0'; var++) {
            char c = *var;
            if (c == '\033') {
                printf("^[");
            }
            else {
                printf("%c", c);
            }
        }
        printf("\n");
    }
    return 0;
}


int handle_nslist(struct josh_line *line)
{
    printf("list of services here\n");
    return 0;
}
