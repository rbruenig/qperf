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
           "  -p              port to listen on/connect to (default 18080)\n"
           "  -s              run as server\n"
           "  -c target       run as client and connect to target server\n"
           "  -t time (s)     run for X seconds (default 10s)\n"
           "  -e              measure time for connection establishment and first byte only\n"
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

    while ((ch = getopt(argc, argv, "p:sc:t:he")) != -1) {
        switch (ch) {
        case 'p':
            port = optarg;
            if(sscanf(optarg, "%u", &port) < 0 || port > 65535) {
                fprintf(stderr, "invalid argument passed to -p\n");
                exit(1);
            }
            break;
        case 's':
            server_mode = true;
            break;
        case 'c':
            host = optarg;
            break;
        case 't':
            if(sscanf(optarg, "%u", &runtime_s) != 1 || runtime_s < 1) {
                fprintf(stderr, "invalid argument passed to -t\n");
                exit(1);
            }
            break;
        case 'e':
            ttfb_only = true;
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
                run_server(port_char, "server.crt", "server.key") :
                run_client(port_char, host, runtime_s, ttfb_only);
}
