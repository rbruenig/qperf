#include "server.h"
#include "server_stream.h"
#include "common.h"

#include <stdio.h>
#include <ev.h>
#include <quicly.h>
#include <quicly/defaults.h>
#include <unistd.h>
#include <float.h>
#include <inttypes.h>
#include <stdbool.h>

#include <quicly/streambuf.h>

#include <picotls/openssl.h>
#include <picotls/../../t/util.h>

static quicly_conn_t **conns;
static int server_socket = -1;
static quicly_context_t server_ctx;
static int server_socket;
static size_t num_conns = 0;
static ev_timer server_timeout;
static quicly_cid_plaintext_t next_cid;

static int udp_listen(struct addrinfo *addr)
{
    for(const struct addrinfo *rp = addr; rp != NULL; rp = rp->ai_next) {
        int s = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if(s == -1) {
            continue;
        }

        int on = 1;
        if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) != 0) {
            close(s);
            perror("setsockopt(SO_REUSEADDR) failed");
            return -1;
        }

        if(bind(s, rp->ai_addr, rp->ai_addrlen) == 0) {
            return s; // success
        }

        // fail -> close socket and try with next addr
        close(s);
    }

    return -1;
}

static inline quicly_conn_t *find_conn(struct sockaddr *sa, socklen_t salen, quicly_decoded_packet_t *packet)
{
    for(size_t i = 0; i < num_conns; ++i) {
        if(quicly_is_destination(conns[i], sa, salen, packet)) {
            return conns[i];
        }
    }
    return NULL;
}

static void append_conn(quicly_conn_t *conn)
{
    ++num_conns;
    conns = realloc(conns, sizeof(quicly_conn_t*) * num_conns);
    assert(conns != NULL);
    conns[num_conns - 1] = conn;

    *quicly_get_data(conn) = calloc(1, sizeof(int64_t));
}

static size_t remove_conn(size_t i)
{
    free(*quicly_get_data(conns[i]));
    quicly_free(conns[i]);
    memmove(conns + i, conns + i + 1, (num_conns - i - 1) * sizeof(quicly_conn_t*));
    --num_conns;
    return i - 1;
}

//void server_report_cb(EV_P_ ev_timer *w, int revents)
//{
//    for(size_t i = 0; i < num_conns; ++i) {
//        quicly_conn_t* conn = conns[i];
//        quic_conn_meta *meta = *quicly_get_data(conn);
//        printf("connection %zu second %i send window: %"PRIi64"\n", i, meta->report_second, meta->send_cwnd);
//        meta->report_second++;
//        fflush(stdout);
//    }
//}

static void server_timeout_cb(EV_P_ ev_timer *w, int revents);

void server_send_pending()
{
    int64_t next_timeout = INT64_MAX;
    for(size_t i = 0; i < num_conns; ++i) {
        if(!send_pending(&server_ctx, server_socket, conns[i])) {
            i = remove_conn(i);
        } else {
            next_timeout = min_int64(quicly_get_first_timeout(conns[i]), next_timeout);
        }
    }

    int64_t now = server_ctx.now->cb(server_ctx.now);
    int64_t timeout = clamp_int64(next_timeout - now, 1, 200);
    server_timeout.repeat = timeout / 1000.;
    ev_timer_again(EV_DEFAULT, &server_timeout);
}

static void server_timeout_cb(EV_P_ ev_timer *w, int revents)
{
    server_send_pending();
}

static inline void server_handle_packet(quicly_decoded_packet_t *packet, struct sockaddr *sa, socklen_t salen)
{
    quicly_conn_t *conn = find_conn(sa, salen, packet);
    if(conn == NULL) {
        // new conn
        int ret = quicly_accept(&conn, &server_ctx, sa, salen, packet, ptls_iovec_init(NULL, 0), &next_cid, NULL);
        if(ret != 0) {
            if(ret == QUICLY_TRANSPORT_ERROR_VERSION_NEGOTIATION) {
                printf("quicly_accept failed with QUICLY_TRANSPORT_ERROR_VERSION_NEGOTIATION\n");
            } else {
                printf("quicly_accept failed with code %i\n", ret);
            }
            return; // ignore errors, including QUICLY_TRANSPORT_ERROR_VERSION_NEGOTIATION
        }
        ++next_cid.master_id;
        printf("got new connection\n");
        append_conn(conn);
    } else {
        int ret = quicly_receive(conn, packet);
        if(ret != 0 && ret != QUICLY_ERROR_PACKET_IGNORED) {
            fprintf(stderr, "quicly_receive returned %i\n", ret);
            exit(1);
        }
    }
}

static void server_read_cb(EV_P_ ev_io *w, int revents)
{
    // retrieve data
    uint8_t buf[4096];
    struct sockaddr sa;
    socklen_t salen = sizeof(sa);
    quicly_decoded_packet_t packet;
    ssize_t bytes_received;

    while((bytes_received = recvfrom(w->fd, buf, sizeof(buf), MSG_DONTWAIT, &sa, &salen)) != -1) {
        for(ssize_t offset = 0; offset < bytes_received; ) {
            size_t packet_len = quicly_decode_packet(&server_ctx, &packet, buf + offset, bytes_received - offset);
            if(packet_len == SIZE_MAX) {
                break;
            }
            server_handle_packet(&packet, &sa, salen);
            offset += packet_len;
        }
    }

    if(errno != EWOULDBLOCK && errno != 0) {
        perror("recvfrom failed");
    }

    server_send_pending();
}

static void server_on_conn_close(quicly_closed_by_peer_t *self, quicly_conn_t *conn, int err,
                                 uint64_t frame_type, const char *reason, size_t reason_len)
{

}

static void event_log(quicly_event_logger_t *_self, quicly_event_type_t type,
                      const quicly_event_attribute_t *attributes, size_t num_attributes)
{
    // this is a hack to get the current send_cwnd of a quicly_conn_t
    // this is necessary since quicly doesn't provide a quicly_get_send_cwnd(conn) method

    int64_t cwnd = INT64_MAX;
    int64_t master_id = INT64_MAX;

    for (size_t i = 0; i != num_attributes; ++i) {
        const quicly_event_attribute_t *attr = attributes + i;
        switch(attr->type) {
        case QUICLY_EVENT_ATTRIBUTE_CONNECTION:
            master_id = attr->value.i;
        case QUICLY_EVENT_ATTRIBUTE_CWND:
            cwnd = attr->value.i;
            break;
        default:
            break;
        }
        if(attr->type == QUICLY_EVENT_ATTRIBUTE_CWND) {
            cwnd = attr->value.i;
        }
    }

    if(cwnd == INT64_MAX || master_id == INT64_MAX) {
        return;
    }

    for(size_t i = 0; i < num_conns; ++i) {
        if(master_id == (int64_t)quicly_get_master_id(conns[i])->master_id) {
            int64_t *send_cwnd = *quicly_get_data(conns[i]);
            *send_cwnd = cwnd;
            return;
        }
    }
}

static quicly_stream_open_t stream_open = {&server_on_stream_open};
static quicly_closed_by_peer_t closed_by_peer = {&server_on_conn_close};
static quicly_event_logger_t event_logger = {&event_log};

int run_server(const char *cert, const char *key)
{
    setup_session_cache(get_tlsctx());
    quicly_amend_ptls_context(get_tlsctx());

    server_ctx = quicly_spec_context;
    server_ctx.tls = get_tlsctx();
    server_ctx.stream_open = &stream_open;
    server_ctx.closed_by_peer = &closed_by_peer;
    server_ctx.transport_params.max_stream_data.uni = UINT32_MAX;
    server_ctx.transport_params.max_stream_data.bidi_local = UINT32_MAX;
    server_ctx.transport_params.max_stream_data.bidi_remote = UINT32_MAX;

    server_ctx.event_log.mask =
            (UINT64_C(1) << QUICLY_EVENT_TYPE_PTO) |
            (UINT64_C(1) << QUICLY_EVENT_TYPE_CC_ACK_RECEIVED) |
            (UINT64_C(1) << QUICLY_EVENT_TYPE_CC_CONGESTION) ;
    server_ctx.event_log.cb = &event_logger;

    load_certificate_chain(server_ctx.tls, cert);
    load_private_key(server_ctx.tls, key);

    struct ev_loop *loop = EV_DEFAULT;

    struct addrinfo *addr = get_address("0.0.0.0", "18080");
    if(addr == NULL) {
        perror("failed get addrinfo for port 18080");
        return -1;
    }

    server_socket = udp_listen(addr);
    freeaddrinfo(addr);
    if(server_socket == -1) {
        perror("failed to listen on port 18080");
        return 1;
    }

    printf("starting server on port 18080\n");

    ev_io socket_watcher;
    ev_io_init(&socket_watcher, &server_read_cb, server_socket, EV_READ);
    ev_io_start(loop, &socket_watcher);

    ev_init(&server_timeout, &server_timeout_cb);

//    ev_timer server_report_timer;
//    ev_timer_init(&server_report_timer, &server_report_cb, 1, 1);
//    ev_timer_start(loop, &server_report_timer);

    ev_run(loop, 0);
    return 0;
}

