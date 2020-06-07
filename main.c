#include <getopt.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#include "server.h"
#include "client.h"


static void usage(const char *cmd)
{
    printf("Usage: %s [options]\n"
            "\n"
            "Options:\n"
            "  -c target       run as client and connect to target server\n"
            "  -e              measure time for connection establishment and first byte only\n"
            "  -g              enable UDP generic segmentation offload\n"
            "  -p              port to listen on/connect to (default 18080)\n"
            "  -s              run as server\n"
            "  -t time (s)     run for X seconds (default 10s)\n"
            "  -h              print this help\n"
            "\n",
           cmd);
}

int main(int argc, char** argv)
{
    int port = 18080;
    bool server_mode = false;
    const char *host = NULL;
    int runtime_s = 10;
    int ch;
    bool ttfb_only = false;
    bool gso = false;

    while ((ch = getopt(argc, argv, "c:egp:st:h")) != -1) {
        switch (ch) {
        case 'c':
            host = optarg;
            break;
        case 'e':
            ttfb_only = true;
            break;
        case 'g':
            #ifdef __linux__
                gso = true;
                printf("using UDP GSO, requires kernel >= 4.18\n");
            #else
                fprintf(stderr, "UDP GSO only supported on linux\n");
                exit(1);
            #endif
            break;
        case 'p':
            port = (intptr_t)optarg;
            if(sscanf(optarg, "%u", &port) < 0 || port > 65535) {
                fprintf(stderr, "invalid argument passed to -p\n");
                exit(1);
            }
            break;
        case 's':
            server_mode = true;
            break;
        case 't':
            if(sscanf(optarg, "%u", &runtime_s) != 1 || runtime_s < 1) {
                fprintf(stderr, "invalid argument passed to -t\n");
                exit(1);
            }
            break;
        default:
            usage(argv[0]);
            exit(1);
        }
    }

    if(server_mode && host != NULL) {
        printf("cannot use -c in server mode\n");
        exit(1);
    }

    if(!server_mode && host == NULL) {
        usage(argv[0]);
        exit(1);
    }

    char port_char[16];
    sprintf(port_char, "%d", port);
    return server_mode ?
                run_server(port_char, gso, "server.crt", "server.key") :
                run_client(port_char, gso, host, runtime_s, ttfb_only);
}