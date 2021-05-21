#include <aos/aos_rpc.h>
#include <aos/dispatcher_rpc.h>
#include <aos/default_interfaces.h>

#include <linenoise/linenoise.h>

#include <stdio.h>

#include "ast.h"
#include "builtins.h"
#include "running.h"

enum shell_state current_state;

struct josh_line *parsed_line;


static errval_t call_spawn_request(const char *name, struct array_list *args, struct array_list *envp, struct running_program *prog)
{
    errval_t err;
    size_t bytes_needed = sizeof(struct spawn_request_header);
    bytes_needed += sizeof(struct spawn_request_header) + strlen(name) + 1;
    for (size_t i = 0; i < args->length; i++) {
        const char *arg = *(char **) array_list_at(args, i);
        bytes_needed += sizeof(struct spawn_request_header) + strlen(arg) + 1;
    }

    void *data = malloc(bytes_needed);
    struct spawn_request_header *header = data;
    size_t offset = sizeof(struct spawn_request_header);

    header->argc = args->length + 1;
    header->envc = 0;

    struct spawn_request_arg *arg_hdr1 = data + offset;

    arg_hdr1->length = strlen(name) + 1;
    memcpy(arg_hdr1->str, name, arg_hdr1->length);
    offset += sizeof(struct spawn_request_header) + arg_hdr1->length;

    for (size_t i = 0; i < args->length; i++) {
        struct spawn_request_arg *arg_hdr = data + offset;
        const char *arg = *(char **) array_list_at(args, i);
        arg_hdr->length = strlen(arg) + 1;
        memcpy(arg_hdr->str, arg, arg_hdr->length);
        offset += sizeof(struct spawn_request_header) + arg_hdr->length;
    }

    struct aos_rpc_varbytes bytes = {
        .length = bytes_needed,
        .bytes = data
    };

    struct lmp_endpoint *lmp_ep;
    struct capref lmp_ep_cap;
    err = endpoint_create(LMP_RECV_LENGTH * 8, &lmp_ep_cap, &lmp_ep);
    ON_ERR_RETURN(err);

    struct aos_rpc *init_rpc = get_init_rpc();
    uintptr_t pid;
    err = aos_rpc_call(init_rpc, INIT_IFACE_SPAWN_EXTENDED, bytes, 0, lmp_ep_cap, &pid);

    free(data); // not needed anymore

    if (err_is_fail(err)) {
        debug_printf("failed to call init\n");

        lmp_endpoint_free(lmp_ep);
        cap_destroy(lmp_ep_cap);
        return LIB_ERR_RPC_NOT_CONNECTED;
    }

    if (pid == -1) {
        lmp_endpoint_free(lmp_ep);
        cap_destroy(lmp_ep_cap);
        return SPAWN_ERR_FIND_MODULE;
    }

    err = aos_rpc_init_lmp(&prog->process_disprpc, lmp_ep_cap, NULL_CAP, lmp_ep, get_default_waitset());
    if (err_is_fail(err)) {
        lmp_endpoint_free(lmp_ep);
        cap_destroy(lmp_ep_cap);
        return err;
    }

    return SYS_ERR_OK;
}

static void spawn_program(const char *name, struct array_list *args)
{
    errval_t err;
    struct running_program program;

    err = call_spawn_request(name, args, NULL, &program);
    if (err == SPAWN_ERR_FIND_MODULE) {
        printf("no module with name '%s' found\n", name);
        return;
    }

    aos_dc_init_lmp(&program.process_in, 1024);
    aos_dc_init_lmp(&program.process_out, 1024);
    endpoint_create(LMP_RECV_LENGTH * 64, &program.process_out.channel.lmp.local_cap, &program.process_out.channel.lmp.endpoint);

    void haendl(struct aos_rpc *r, struct capref ep, struct capref stdinep, struct capref *stdoutep) {
        r->channel.lmp.remote_cap = ep;
        program.process_in.channel.lmp.remote_cap = stdinep;
        *stdoutep = program.process_out.channel.lmp.local_cap;
    }

    struct aos_rpc *rpc = &program.process_disprpc;
    aos_rpc_set_interface(rpc, get_dispatcher_interface(), DISP_IFACE_N_FUNCTIONS, malloc(DISP_IFACE_N_FUNCTIONS * sizeof (void *)));
    aos_rpc_register_handler(rpc, DISP_IFACE_BINDING, haendl);
    
    //struct capref otherep;

    while(capref_is_null(rpc->channel.lmp.remote_cap)) {
        err = event_dispatch(get_default_waitset());
    }

    process_running(&program);

    aos_dc_free(&program.process_in);
    aos_dc_free(&program.process_out);
    aos_rpc_free(&program.process_disprpc);
    printf("\n");
}



static void execute_command(struct josh_line *line)
{
    if (is_builtin(line->cmd)) {
        run_builtin(line);
    }
    else {
        spawn_program(line->cmd, &line->args);
    }
}

static void process_line(void) {
    char *line = linenoise(getenv("PROMPT"));

    if (line == NULL || line[0] == '\0') {
        return;
    }

    linenoiseHistoryAdd(line);

    //debug_printf("we got line: %s\n", line);
    yylex_destroy(); // reset parser
    yy_scan_string(line);
    int errval = yyparse();
    if (errval == 0 && parsed_line != NULL) {
        execute_command(parsed_line);
        josh_line_free(parsed_line);
        parsed_line = NULL;
    }
    else {
        printf("error parsing command!\n");
    }
    linenoiseFree(line);
}


static void complete_line(const char *line, linenoiseCompletions *completions)
{
    completions->len = 0;
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


    linenoiseHistorySetMaxLen(64);
    linenoiseSetCompletionCallback(&complete_line);

    while(true) {
        process_line();
    }

    //print_prompt();

    //lmp_chan_register_recv(&stdin_chan.channel.lmp, get_default_waitset(), MKCLOSURE(on_input, &stdin_chan));
}
