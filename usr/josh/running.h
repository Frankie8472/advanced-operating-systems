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
    domainid_t domainid;

    struct capref out_cap;
    struct aos_datachan process_out;

    struct capref in_cap;
    struct aos_datachan process_in;

    struct aos_rpc process_disprpc;
};


void display_running_process(struct running_program *input_receiver, struct aos_datachan *to_print);

#endif // JOSH_RUNNING_H
