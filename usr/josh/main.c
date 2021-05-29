#include <aos/aos_rpc.h>
#include <aos/dispatcher_rpc.h>
#include <aos/default_interfaces.h>

#include <linenoise/linenoise.h>

#include <stdio.h>

#include "ast.h"
#include "builtins.h"
#include "running.h"
#include "variables.h"



enum shell_state current_state;

struct josh_line *parsed_line;


static errval_t setup_ump_channels(struct running_program *prog)
{
    errval_t err;
    bool has_foreign_out = true;
    bool has_foreign_in = true;

    if (capref_is_null(prog->out_cap)) {
        frame_alloc(&prog->out_cap, BASE_PAGE_SIZE, NULL);
        has_foreign_out = false;
    }

    if (capref_is_null(prog->in_cap)) {
        frame_alloc(&prog->in_cap, BASE_PAGE_SIZE, NULL);
        has_foreign_in = false;
    }

    if (!has_foreign_in || true) {
        void *stdin_addr;
        err = paging_map_frame_complete(get_current_paging_state(), &stdin_addr, prog->out_cap, NULL, NULL);
        err = aos_dc_init_ump(&prog->process_out, 64, (lvaddr_t) stdin_addr, BASE_PAGE_SIZE, 1);
        ON_ERR_RETURN(err);
    }
    else {
        debug_printf("has fi\n");
    }

    if (!has_foreign_out || true) {
        void *stdout_addr;
        err = paging_map_frame_complete(get_current_paging_state(), &stdout_addr, prog->in_cap, NULL, NULL);
        err = aos_dc_init_ump(&prog->process_in, 64, (lvaddr_t) stdout_addr, BASE_PAGE_SIZE, 0);
        ON_ERR_RETURN(err);
    }

    return SYS_ERR_OK;
}


static errval_t call_spawn_request(const char *name, coreid_t core, size_t argc, const char **argv, struct array_list *envp, struct running_program *prog)
{
    errval_t err;
    size_t bytes_needed = sizeof(struct spawn_request_header);
    bytes_needed += sizeof(struct spawn_request_header) + strlen(name) + 1;
    for (size_t i = 0; i < argc; i++) {
        const char *arg = argv[i];
        bytes_needed += sizeof(struct spawn_request_header) + strlen(arg) + 1;
    }

    void *data = malloc(bytes_needed);
    struct spawn_request_header *header = data;
    size_t offset = sizeof(struct spawn_request_header);

    header->argc = argc + 1;
    header->envc = 0;

    struct spawn_request_arg *arg_hdr1 = data + offset;

    arg_hdr1->length = strlen(name) + 1;
    memcpy(arg_hdr1->str, name, arg_hdr1->length);
    offset += sizeof(struct spawn_request_header) + arg_hdr1->length;

    for (size_t i = 0; i < argc; i++) {
        struct spawn_request_arg *arg_hdr = data + offset;
        const char *arg = argv[i];
        arg_hdr->length = strlen(arg) + 1;
        memcpy(arg_hdr->str, arg, arg_hdr->length);
        offset += sizeof(struct spawn_request_header) + arg_hdr->length;
    }

    struct aos_rpc_varbytes bytes = {
        .length = bytes_needed,
        .bytes = data
    };



    struct aos_rpc *init_rpc = get_init_rpc();
    uintptr_t pid;

    if (core == disp_get_core_id() && false) {

        struct lmp_endpoint *lmp_ep;
        struct capref lmp_ep_cap;
        err = endpoint_create(LMP_RECV_LENGTH * 8, &lmp_ep_cap, &lmp_ep);
        ON_ERR_RETURN(err);

        err = aos_rpc_call(init_rpc, INIT_IFACE_SPAWN_EXTENDED, bytes, core, lmp_ep_cap, NULL_CAP, NULL_CAP, &pid);

        free(data); // not needed anymore

        if (err_is_fail(err)) {
            debug_printf("failed to call init\n");

            lmp_endpoint_free(lmp_ep);
            cap_destroy(lmp_ep_cap);
            return LIB_ERR_RPC_NOT_CONNECTED;
        }

        prog->domainid = pid;

        if (((int) pid) == MOD_NOT_FOUND || ((int) pid) == COREID_INVALID) {
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

        aos_dc_init_lmp(&prog->process_in, 1024);
        aos_dc_init_lmp(&prog->process_out, 1024);
        endpoint_create(LMP_RECV_LENGTH * 64, &prog->process_out.channel.lmp.local_cap, &prog->process_out.channel.lmp.endpoint);

        void haendl(struct aos_rpc *r, struct capref ep, struct capref stdinep, struct capref *stdoutep) {
            r->channel.lmp.remote_cap = ep;
            prog->process_in.channel.lmp.remote_cap = stdinep;
            *stdoutep = prog->process_out.channel.lmp.local_cap;
        }

        struct aos_rpc *rpc = &prog->process_disprpc;
        aos_rpc_set_interface(rpc, get_dispatcher_interface(), DISP_IFACE_N_FUNCTIONS, malloc(DISP_IFACE_N_FUNCTIONS * sizeof (void *)));
        aos_rpc_register_handler(rpc, DISP_IFACE_BINDING, haendl);

        while(capref_is_null(rpc->channel.lmp.remote_cap)) {
            err = event_dispatch(get_default_waitset());
        }
    }
    else {
        struct capref rpc_frame;
        frame_alloc(&rpc_frame, BASE_PAGE_SIZE, NULL);

        prog->domainid = pid;
        if (((int) pid) == MOD_NOT_FOUND || ((int) pid) == COREID_INVALID) {
            cap_destroy(rpc_frame);
            cap_destroy(prog->out_cap);
            cap_destroy(prog->in_cap);
            return SPAWN_ERR_FIND_MODULE;
        }

        void *disp_rpc_addr;
        err = paging_map_frame_complete(get_current_paging_state(), &disp_rpc_addr, rpc_frame, NULL, NULL);


        err = aos_rpc_init_ump_default(&prog->process_disprpc, (lvaddr_t) disp_rpc_addr, BASE_PAGE_SIZE, 1);
        ON_ERR_RETURN(err);
        aos_rpc_set_interface(&prog->process_disprpc, get_dispatcher_interface(), DISP_IFACE_N_FUNCTIONS, malloc(DISP_IFACE_N_FUNCTIONS * sizeof (void *)));
        
        setup_ump_channels(prog);

        err = aos_rpc_call(init_rpc, INIT_IFACE_SPAWN_EXTENDED, bytes, core, rpc_frame, prog->out_cap, prog->in_cap, &pid);
        free(data); // not needed anymore

        if (err_is_fail(err)) {
            debug_printf("failed to call init\n");
            return LIB_ERR_RPC_NOT_CONNECTED;
        }

        prog->domainid = pid;

        if (((int) pid) == MOD_NOT_FOUND || ((int) pid) == COREID_INVALID) {
            return SPAWN_ERR_FIND_MODULE;
        }
    }

    return SYS_ERR_OK;
}


bool is_number(const char *string)
{
    if (string == NULL) {
        return false;
    }
    for (int i = 0; string[i] != '\0'; i++) {
        if (!isdigit(string[i])) {
            return false;
        }
    }
    return true;
}


static errval_t spawn_program(const char *dest, const char *cmd, size_t argc, const char **argv, struct running_program *prog)
{
    errval_t err;

    coreid_t destination_core = disp_get_core_id();
    if (dest != NULL) {
        if (is_number(dest)) {
            int idest = atoi(dest);
            destination_core = idest;
        }
        else {
            printf("invalid destination '%s'\n", dest);
            return SPAWN_ERR_FIND_MODULE;
        }
    }

    err = call_spawn_request(cmd, destination_core, argc, argv, NULL, prog);
    if (err == SPAWN_ERR_FIND_MODULE) {
        debug_printf("why?\n");
        if (prog->domainid == COREID_INVALID) {
            printf("spawning failed: invalid destination\n", cmd);
        }
        else {
            printf("no module with name '%s' found\n", cmd);
        }
        return SPAWN_ERR_FIND_MODULE;
    }

    return SYS_ERR_OK;
}


static int builtin_threadentry(void *arg)
{
    struct running_program *prog = arg;

    struct aos_datachan builtin_in;
    struct aos_datachan builtin_out;

    if (!capref_is_null(prog->in_cap)) {
        void *page;
        paging_map_frame_complete(get_current_paging_state(), &page,  prog->in_cap, NULL, NULL);
        aos_dc_init_ump(&builtin_in, 64, (lvaddr_t) page, BASE_PAGE_SIZE, 1);
    }

    if (!capref_is_null(prog->out_cap)) {
        void *page;
        paging_map_frame_complete(get_current_paging_state(), &page,  prog->out_cap, NULL, NULL);
        aos_dc_init_ump(&builtin_out, 64, (lvaddr_t) page, BASE_PAGE_SIZE, 0);
    }

    run_builtin(prog->cmd, prog->argc, (const char **) prog->argv, &builtin_out);
    aos_dc_close(&builtin_out);

    return 0;
}


static errval_t setup_builtin(const char *cmd, size_t argc, const char **argv, struct running_program *prog)
{
    prog->is_builtin = true;
    prog->cmd = strdup(cmd);
    prog->argc = argc;
    prog->argv = malloc(argc * sizeof(char *));
    memcpy(prog->argv, argv, argc * sizeof(char *));

    prog->builtin_thread = thread_create(builtin_threadentry, prog);

    setup_ump_channels(prog);
    return SYS_ERR_OK;
}


char *evaluate_value(struct josh_value *value)
{
    if (value->type == JV_LITERAL) {
        return value->val;
    }
    else if (value->type == JV_VARIABLE) {
        char *envvar = getenv(value->val);
        if (envvar) return envvar;
        char *var = get_variable(value->val);
        return var;
    }
    else {
        printf("internal error\n");
        return NULL;
    }
}


static void execute_command(struct josh_command *command)
{
    errval_t err = SYS_ERR_OK;

    struct josh_command *c = command;
    size_t n_programs = 0;
    while(c != NULL) {
        n_programs ++;
        c = c->piped_into;
    }

    struct running_program *programs = malloc(n_programs * sizeof(struct running_program));
    memset(programs, 0, n_programs * sizeof(struct running_program));

    struct capref out_before;
    err = frame_alloc(&out_before, BASE_PAGE_SIZE, NULL);
    for (size_t i = 0; i < n_programs; i++) {
        struct capref frame;
        err = frame_alloc(&frame, BASE_PAGE_SIZE, NULL);
        ON_ERR_NO_RETURN(err);
        programs[i].out_cap = frame;
        if (!capref_is_null(out_before)) {
            programs[i].in_cap = out_before;
        }
        out_before = frame;
    }


    for (size_t j = 0; j < n_programs; j++) {
        const char *cmd = command->cmd;
        size_t argc = command->args.length;
        char **argv = malloc(argc * sizeof(char *));

        for (size_t i = 0; i < argc; i++) {
            struct josh_value *arg = *(struct josh_value **) array_list_at(&command->args, i);
            argv[i] = evaluate_value(arg);

            // create empty string if null
            if (argv[i] == NULL) {
                argv[i] = "";
            }
        }

        const char* destination = command->destination;

        if (is_builtin(cmd)) {
            if (destination != NULL && atoi(destination) != disp_get_core_id()) {
                printf("Can't execute shell builtins on different cores\n");
            }
            else {
                setup_builtin(cmd, argc, (const char **) argv, &programs[j]);
            }
        }
        else {
            err = spawn_program(destination, cmd, argc, (const char **) argv, &programs[j]);
            if (err_is_fail(err)) {
                free(argv);
                break;
            }
        }

        free(argv);
        command = command->piped_into;
    }

    if (err_is_fail(err)) {
        return; // TODO: stop leaking memory & other resources
    }

    display_running_process(&programs[0], &programs[n_programs - 1].process_out);


    for (size_t i = 0; i < n_programs; i++) {
        if (programs[i].is_builtin) {
            err = thread_join(programs[i].builtin_thread, NULL);
            aos_dc_free(&programs[i].process_in);
            aos_dc_free(&programs[i].process_out);
            aos_rpc_free(&programs[i].process_disprpc);
        }
        else {
            aos_dc_free(&programs[i].process_in);
            aos_dc_free(&programs[i].process_out);
            aos_rpc_free(&programs[i].process_disprpc);
        }
        //cap_destroy(programs[i].in_cap);
        //cap_destroy(programs[i].out_cap);
    }
    printf("\n");
}


static void do_assignment(struct josh_assignment *assignment)
{
    const char *varname = assignment->varname;
    const char *value = evaluate_value(assignment->value);
    if (assignment->is_shell_var) {
        set_variable(varname, value);
    }
    else {
        setenv(varname, value, 1);
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
        if (parsed_line->type == JL_COMMAND) {
            struct josh_command *command = parsed_line->command;
            execute_command(command);
        }
        else if (parsed_line->type == JL_ASSIGNMENT) {
            struct josh_assignment *assignment = parsed_line->assignment;
            do_assignment(assignment);
        }
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
    setenv("PROMPT", "\033[32;1mjosh\033[m $ ", 0);
    setenv("PATH", "/usr/bin:/home", 0);

    printf("Welcome to JameOS Shell\n");

    linenoiseHistorySetMaxLen(64);
    linenoiseSetCompletionCallback(&complete_line);

    while(true) {
        process_line();
    }

    return 0;
}
