#include <stdio.h>
#include <stdint.h>

#include <aos/aos.h>
#include <aos/udp_service.h>

int main(int argc, char **argv) {
    char *arp_str = malloc(1024 * sizeof(char));

    aos_arp_table_get(arp_str);
    printf("=========== ARP Table ===========\n"
           "%s"
           "=================================\n",
           arp_str);

    free(arp_str);

    return EXIT_SUCCESS;
}
