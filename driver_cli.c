#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "driver.h"

#define SOCK_RELATIVE "/.texr.sock"

int main(int argc, char **argv) {
    int len, allocated=0;
    char *home, *sock;

    if(argc ==1) {
        home = getenv("HOME");
        len = strlen(home) + sizeof(SOCK_RELATIVE) + 1;
        sock = malloc(len);
        len = snprintf(sock, len, "%s%s", home, SOCK_RELATIVE);
        allocated = 1;
    } else {
        sock = argv[1];
    }

    libresip_driver(sock);

    if(allocated) {
        free(sock);
    }
}

