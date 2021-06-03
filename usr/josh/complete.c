#include <aos/aos.h>
#include <aos/dispatcher_rpc.h>
#include <aos/default_interfaces.h>

#include "complete.h"


static char *comps[256];
static size_t comps_len = 0;
bool comps_initialized = false;


static void initialize_comps(void)
{
    char modules[2048];
    aos_rpc_call(get_init_rpc(), INIT_IFACE_GET_ALL_MODULES, modules);

    // modules contains now a semicolon-separated list of all modules on the core

    size_t comp_idx = 0;
    char *curr_name = modules;
    while(true) {
        char *semic = strchr(curr_name, ';');
        if (semic == NULL) {
            break;
        }
        size_t len = semic - curr_name;
        comps[comp_idx] = malloc(len + 1);
        memcpy(comps[comp_idx], curr_name, len);
        comps[comp_idx][len] = '\0';

        comp_idx++;
        curr_name = semic + 1;
    }

    comps[comp_idx++] = "help";
    comps[comp_idx++] = "echo";
    comps[comp_idx++] = "clear";
    comps[comp_idx++] = "env";
    comps[comp_idx++] = "pmlist";
    comps[comp_idx++] = "nslist";
    comps[comp_idx++] = "nslookup";

    comps_len = comp_idx;
    comps_initialized = true;
    return;
}


void complete_line(const char *line, linenoiseCompletions *completions)
{
    size_t length = strlen(line);


    if (!comps_initialized) {
        initialize_comps();
    }

    completions->len = 0;
    for (int i = 0; i < comps_len; i++) {
        if (strlen(comps[i]) >= length && memcmp(line, comps[i], length) == 0) {
            linenoiseAddCompletion(completions, comps[i]);
        }
    }
}
