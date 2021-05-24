#include "builtins.h"
#include "format.h"

#include <aos/nameserver.h>
#include <aos/aos_rpc.h>
#include <aos/default_interfaces.h>
struct builtin
{
    const char *cmd;
    int(*handler)(struct josh_command *line);
};


int handle_echo(struct josh_command *line);
int handle_env(struct josh_command *line);
int handle_pmlist(struct josh_command *line);
int handle_nslist(struct josh_command *line);

const struct builtin builtins[] = {
    { "echo", &handle_echo },
    { "env", &handle_env },
    { "pmlist", &handle_pmlist },
    { "nslist", &handle_nslist}
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


int run_builtin(struct josh_command *line)
{
    for (size_t i = 0; i < sizeof(builtins) / sizeof(builtins[0]); i++) {
        if (strcmp(line->cmd, builtins[i].cmd) == 0) {
            return builtins[i].handler(line);
        }
    }
    return 1;
}


int handle_echo(struct josh_command *line)
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
int handle_env(struct josh_command *line)
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



int handle_pmlist(struct josh_command *line)
{
    errval_t err;
    size_t pid_count;
    domainid_t *pids;
    err = aos_rpc_process_get_all_pids(get_ns_rpc(), &pids, &pid_count);

    if (err_is_fail(err)) {
        printf("error querying nameserver\n");
        return 1;
    }

    printf(JF_BOLD "%-9s %-32s\n" JF_RESET, "PID", "Name");
    for (size_t i = 0; i < pid_count; i++) {
        char *pname;
        err = aos_rpc_process_get_name(get_ns_rpc(), pids[i], &pname);
        if (err_is_ok(err)) {
            printf("%-9"PRIuDOMAINID" %-32s\n", pids[i], pname);
        }
        else {
            printf("error querying process name\n");
        }

        free(pname);
    }
    free(pids);
    return 0;
}


int handle_nslist(struct josh_command *line){
    // nslist "/hello/" "type=ethernet,speed="
    errval_t err;
    // char * ret_string[2000]; // MAX AMOUNT OF SERVERS
    char ** ret_string = (char **) malloc(1024 * sizeof(char*));
    size_t ret_size;
    err = nameservice_enumerate("/",&ret_size,ret_string);
    // if(err_is_ok)
    if(err_is_fail(err) || ret_size >= 1024){
        printf("error querying nameserver\n");
        DEBUG_ERR(err,"");
        return 1;
    }

    printf(JF_BOLD "%-32s # %-9"PRIuDOMAINID"\n" JF_RESET, "Servers",ret_size);
    for(int i = 0;i < ret_size;++i){
        printf("%s\n",ret_string[i]);
    }


    free(ret_string);

    return 0;

}