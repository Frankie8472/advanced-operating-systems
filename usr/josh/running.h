#ifndef JOSH_RUNNING_H
#define JOSH_RUNNING_H

#include <aos/aos_rpc.h>
#include <aos/aos_datachan.h>

enum shell_state {
    PROMPTING,
    FORWARDING,
};


struct running_program
{
    struct aos_datachan process_out;
    struct aos_datachan process_in;
    struct aos_rpc process_disprpc;
};


void process_running(struct running_program *prog);

#endif // JOSH_RUNNING_H
