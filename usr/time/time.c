#include <aos/aos.h>
#include <aos/aos_rpc.h>
#include <aos/systime.h>
#include <aos/aos_datachan.h>
#include <aos/default_interfaces.h>


static void handle_out(void *arg)
{
    struct aos_datachan *out = arg;

    char buf[1024];
    size_t recvd;
    aos_dc_receive(out, sizeof buf, buf, &recvd);

    debug_printf("%s\n", buf);

    aos_dc_register(out, get_default_waitset(), MKCLOSURE(handle_out, &out));
}

int main(int argc, char **argv)
{
    size_t arg_req_len = 0;
    for (int i = 1; i < argc; i++) {
        arg_req_len += strlen(argv[i]) + 1 + sizeof(size_t);
    }

    struct spawn_request_header header = {
        .argc = argc,
        .envc = 0
    };

    char *bytes = malloc(sizeof header + arg_req_len);
    char *curr_ptr = bytes;

    memcpy(curr_ptr, &header, sizeof header);
    curr_ptr += sizeof header;

    for (int i = 1; i < argc; i++) {
        struct spawn_request_arg *arg = (struct spawn_request_arg *) curr_ptr;
        arg->length = strlen(argv[i] + 1);
        memcpy(arg->str, argv[i], arg->length);
        curr_ptr += strlen(argv[i]) + 1 + sizeof(size_t);
    }

    struct aos_rpc_varbytes sendbytes = {
        .length = sizeof header + arg_req_len,
        .bytes = bytes
    };
    uintptr_t pid;

    size_t retsize;
    struct capref rpc_frame;
    struct capref out_cap;
    struct capref in_cap;

    void *rpc_addr;
    void *out_addr;
    void *in_addr;

    frame_alloc_and_map(&rpc_frame, BASE_PAGE_SIZE, &retsize, &rpc_addr);
    frame_alloc_and_map(&out_cap, BASE_PAGE_SIZE, &retsize, &out_addr);
    frame_alloc_and_map(&in_cap, BASE_PAGE_SIZE, &retsize, &in_addr);

    struct aos_datachan out;
    struct aos_datachan in;

    aos_dc_init_ump(&out, 64, (lvaddr_t) out_addr, BASE_PAGE_SIZE, 1);
    aos_dc_init_ump(&in, 64, (lvaddr_t) in_addr, BASE_PAGE_SIZE, 0);

    aos_dc_register(&out, get_default_waitset(), MKCLOSURE(handle_out, &out));


    systime_t begin = systime_now();
    aos_rpc_call(get_init_rpc(), INIT_IFACE_SPAWN_EXTENDED, sendbytes, disp_get_core_id(), rpc_frame, out_cap, in_cap, &pid);

    systime_t end = systime_now();
    printf("time: %ld us\n", systime_to_us(end - begin));

    free(bytes);

    return 0;
}