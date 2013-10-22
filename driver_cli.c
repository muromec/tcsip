#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "driver.h"

#define SOCK_RELATIVE "/.texr.sock"

int main(int argc, char **argv) {
    int len;
    char *home, *sock;

    if(argc ==1) {
        home = getenv("HOME");
        len = strlen(home) + sizeof(SOCK_RELATIVE) + 1;
        sock = malloc(len);
        len = snprintf(sock, len, "%s%s", home, SOCK_RELATIVE);
    } else {
        sock = argv[1];
    }

    libresip_driver(sock);

    free(sock);
}

