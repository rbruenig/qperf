#include "common.h"

#include <sys/socket.h>
#include <netinet/udp.h>
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

bool send_dgrams_default(int fd, struct sockaddr *dest, struct iovec *dgrams, size_t num_dgrams)
{
    for(size_t i = 0; i < num_dgrams; ++i) {
        struct msghdr mess = {
            .msg_name = dest,
            .msg_namelen = quicly_get_socklen(dest),
            .msg_iov = &dgrams[i], .msg_iovlen = 1
        };

        ssize_t bytes_sent;
        while ((bytes_sent = sendmsg(fd, &mess, 0)) == -1 && errno == EINTR);
        if (bytes_sent == -1) {
            perror("sendmsg failed");
            return false;
        }
    }

    return true;
}

#ifdef __linux__
    /* UDP GSO is only supported on linux */
    #ifndef UDP_SEGMENT
        #define UDP_SEGMENT 103 /* Set GSO segmentation size */
    #endif

bool send_dgrams_gso(int fd, struct sockaddr *dest, struct iovec *dgrams, size_t num_dgrams)
{
    struct iovec vec = {
        .iov_base = (void *)dgrams[0].iov_base,
        .iov_len = dgrams[num_dgrams - 1].iov_base + dgrams[num_dgrams - 1].iov_len - dgrams[0].iov_base
    };

    struct msghdr mess = {
        .msg_name = dest,
        .msg_namelen = quicly_get_socklen(dest),
        .msg_iov = &vec,
        .msg_iovlen = 1
    };

    union {
        struct cmsghdr hdr;
        char buf[CMSG_SPACE(sizeof(uint16_t))];
    } cmsg;
    if (num_dgrams != 1) {
        cmsg.hdr.cmsg_level = SOL_UDP;
        cmsg.hdr.cmsg_type = UDP_SEGMENT;
        cmsg.hdr.cmsg_len = CMSG_LEN(sizeof(uint16_t));
        *(uint16_t *)CMSG_DATA(&cmsg.hdr) = dgrams[0].iov_len;
        mess.msg_control = &cmsg;
        mess.msg_controllen = (socklen_t)CMSG_SPACE(sizeof(uint16_t));
    }

    ssize_t bytes_sent;
    while ((bytes_sent = sendmsg(fd, &mess, 0)) == -1 && errno == EINTR);
    if (bytes_sent == -1) {
        perror("sendmsg failed");
        return false;
    }

    return true;
}

#endif

bool (*send_dgrams)(int fd, struct sockaddr *dest, struct iovec *dgrams, size_t num_dgrams) = send_dgrams_default;

void enable_gso()
{
    send_dgrams = send_dgrams_gso;
}

bool send_pending(quicly_context_t *ctx, int fd, quicly_conn_t *conn)
{
    #define SEND_BATCH_SIZE 16

    quicly_address_t dest, src;
    struct iovec dgrams[SEND_BATCH_SIZE];
    uint8_t dgrams_buf[SEND_BATCH_SIZE * ctx->transport_params.max_udp_payload_size];
    size_t num_dgrams = SEND_BATCH_SIZE;

    while(true) {
        int quicly_res = quicly_send(conn, &dest, &src, dgrams, &num_dgrams, &dgrams_buf, sizeof(dgrams_buf));
        if(quicly_res != 0) {
            if(quicly_res != QUICLY_ERROR_FREE_CONNECTION) {
                printf("quicly_send failed with code %i\n", quicly_res);
            } else {
                printf("connection closed\n");
            }
            return false;
        } else if(num_dgrams == 0) {
            return true;
        }

        if (!send_dgrams(fd, &dest.sa, dgrams, num_dgrams)) {
            return false;
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

