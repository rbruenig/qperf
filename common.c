#include "common.h"

#include <sys/socket.h>
#include <netdb.h>
#include <memory.h>
#include <picotls/openssl.h>
#include <errno.h>


ptls_context_t *get_tlsctx()
{
    static ptls_context_t tlsctx = {.random_bytes = ptls_openssl_random_bytes,
                                    .get_time = &ptls_get_time,
                                    .key_exchanges = ptls_openssl_key_exchanges,
                                    .cipher_suites = ptls_openssl_cipher_suites,
                                    .require_dhe_on_psk = 1};
    return &tlsctx;
}

struct addrinfo *get_address(const char *host, const char *port)
{
    struct addrinfo hints;
    struct addrinfo *result;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;                    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_DGRAM;                 /* Datagram socket */
    hints.ai_flags = AI_ADDRCONFIG | AI_NUMERICSERV | AI_PASSIVE;
    hints.ai_protocol = IPPROTO_UDP;

    if(getaddrinfo(host, port, &hints, &result) != 0) {
        return NULL;
    } else {
        return result;
    }
}

bool send_pending(quicly_context_t *ctx, int fd, quicly_conn_t *conn)
{
#define SEND_BATCH_SIZE 16

    quicly_datagram_t   *packets[SEND_BATCH_SIZE];

    while(true) {
        size_t packet_count = SEND_BATCH_SIZE;
        int quicly_res = quicly_send(conn, packets, &packet_count);
        if(quicly_res != 0) {
            if(quicly_res != QUICLY_ERROR_FREE_CONNECTION) {
                printf("quicly_send failed with code %i\n", quicly_res);
            } else {
                printf("connection closed\n");
            }
            return false;
        } else if(packet_count == 0) {
            return true;
        }

        for(size_t i = 0; i < packet_count; ++i) {
            ssize_t bytes_sent = sendto(fd, packets[i]->data.base, packets[i]->data.len, 0, &packets[i]->sa, packets[i]->salen);
            ctx->packet_allocator->free_packet(ctx->packet_allocator, packets[i]);
            if(bytes_sent == -1) {
                perror("sendto failed");
                return false;
            }
        }
    };
}


void print_escaped(const char *src, size_t len)
{
    for(size_t i = 0; i < len; ++i) {
        switch (src[i]) {
        case '\n':
            putchar('\\');
            putchar('n');
            break;
        case '\r':
            putchar('\\');
            putchar('r');
            break;
        default:
            putchar(src[i]);
        }
    }
    putchar('\n');
    fflush(stdout);
}

