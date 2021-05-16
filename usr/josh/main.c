#include <aos/aos_rpc.h>
#include <aos/dispatcher_rpc.h>
#include <aos/default_interfaces.h>

#include <linenoise/linenoise.h>

#include <stdio.h>

#include "ast.h"
#include "builtins.h"


struct josh_line *parsed_line;


void execute_command(struct josh_line *line)
{
    if (is_builtin(line->cmd)) {
        run_builtin(line);
    }
    else {
        struct aos_rpc *init_rpc = get_init_rpc();
        uintptr_t pid;
        aos_rpc_call(init_rpc, INIT_IFACE_SPAWN, line->cmd, 0, &pid);
        //printf("unknown command: %s\n", line->cmd);
    }
}


static void process_line(void) {
    char *line = linenoise(getenv("PROMPT"));

    if (line == NULL || line[0] == '\0') {
        return;
    }

    linenoiseHistoryAdd(line);

    //debug_printf("we got line: %s\n", line);
    yy_scan_string(line);
    int errval = yyparse();
    if (errval == 0) {
        execute_command(parsed_line);
        josh_line_free(parsed_line);
        parsed_line = NULL;
    }
    else {
        printf("error parsing command!\n");
    }
    free(line);
}

int main(int argc, char **argv)
{
    errval_t err;
    struct waitset *default_ws = get_default_waitset();
    err = event_dispatch(default_ws);
    err = event_dispatch(default_ws);
    err = event_dispatch(default_ws);

    printf("Welcome to JameOS Shell\n");

    setenv("PROMPT", "\033[32;1mjosh\033[m $ ", 0);

    while(true) {
        process_line();
    }

    //print_prompt();

    //lmp_chan_register_recv(&stdin_chan.channel.lmp, get_default_waitset(), MKCLOSURE(on_input, &stdin_chan));

    while (true) {
        err = event_dispatch(default_ws);
        if (err_is_fail(err)) {
            DEBUG_ERR(err, "in event_dispatch");
            abort();
        }
    }
}
