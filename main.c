#include <getopt.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
 #include <sys/wait.h>
#include "server.h"
#include "client.h"


static void usage(const char *cmd)
{
    printf("Usage: %s [options]\n"
            "\n"
            "Options:\n"
            "  -c target            run as client and connect to target server\n"
            "  --cc [reno,cubic]    congestion control algorithm to use (default reno)\n"
            "  -e                   measure time for connection establishment and first byte only\n"
            "  -g                   enable UDP generic segmentation offload\n"
            "  --iw initial-window  initial window to use (default 10)\n"
            "  -l log-file          file to log tls secrets\n"
            "  -p                   port to listen on/connect to (default 18080)\n"
            "  -s  address          listen as server on address\n"
            "  -t time (s)          run for X seconds (default 10s)\n"
            "  -n num_cores         number of cores to use (default nproc)\n"
            "  -h                   print this help\n"
            "\n",
           cmd);
}

static struct option long_options[] = 
{
    {"cc", required_argument, NULL, 0},
    {"iw", required_argument, NULL, 1},
    {NULL, 0, NULL, 0}
};

int main(int argc, char** argv)
{
    int port = 18080;
    bool server_mode = false;
    const char *host = NULL;
    const char *address = NULL;
    int runtime_s = 10;
    int ch;
    bool ttfb_only = false;
    bool gso = false;
    const char *logfile = NULL;
    const char *cc = "reno";
    int iw = 10;
    int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    if (num_cores < 1) {
        num_cores = 1;
    }

    while ((ch = getopt_long(argc, argv, "c:egl:p:s:t:n:h", long_options, NULL)) != -1) {
        switch (ch) {
        case 0:
            if(strcmp(optarg, "reno") != 0 && strcmp(optarg, "cubic") != 0) {
                fprintf(stderr, "invalid argument passed to --cc\n");
                exit(1);
            }
            cc = optarg;
            break;
        case 1:
            iw = (intptr_t)optarg;
            if (sscanf(optarg, "%" SCNu32, &iw) != 1) {
                fprintf(stderr, "invalid argument passed to --iw\n");
                exit(1);
            }
            break;
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
        case 'l':
            logfile = optarg;
            break;
        case 'p':
            // 
            if(sscanf(optarg, "%u", &port) < 0 || port > 65535) {
                fprintf(stderr, "invalid argument passed to -p\n");
                exit(1);
            }
            break;
        case 's':
            address = optarg;
            server_mode = true;
            break;
        case 't':
            if(sscanf(optarg, "%u", &runtime_s) != 1 || runtime_s < 1) {
                fprintf(stderr, "invalid argument passed to -t\n");
                exit(1);
            }
            break;
        case 'n':
            if(sscanf(optarg, "%u", &num_cores) != 1 || num_cores < 1) {
                fprintf(stderr, "invalid argument passed to -n\n");
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

    pid_t *child_pids = malloc(num_cores * sizeof(pid_t));
    if (!child_pids) {
        fprintf(stderr, "failed to allocate memory for child pids\n");
        exit(1);
    }
    for (int i = 0; i < num_cores; i++) {
        int new_port = port + i;
        char new_port_char[16];
        sprintf(new_port_char, "%d", new_port);

        pid_t pid = fork();
        if (pid < 0) {
            fprintf(stderr, "fork failed\n");
            exit(1);
        } else if (pid == 0) {
            // Child process: run as server or client with new_port
            if (server_mode) {
                exit(run_server(address, new_port_char, gso, logfile, cc, iw, "server.crt", "server.key"));
            } else {
                exit(run_client(new_port_char, gso, logfile, cc, iw, host, runtime_s, ttfb_only));
            }
        } else {
            child_pids[i] = pid;
        }
    }

    // Parent process waits for all child processes to finish.
    for (int i = 0; i < num_cores; i++) {
        int status;
        waitpid(child_pids[i], &status, 0);
    }
    free(child_pids);
    return 0;
}