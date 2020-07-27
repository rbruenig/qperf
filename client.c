#include "client.h"
#include "client_stream.h"
#include "common.h"

#include <ev.h>
#include <stdio.h>
#include <quicly.h>
#include <quicly/defaults.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <float.h>
#include <stdbool.h>

#include <quicly/streambuf.h>

#include <picotls/../../t/util.h>

static int client_socket = -1;
static quicly_conn_t *conn = NULL;
static ev_timer client_timeout;
static quicly_context_t client_ctx;
static quicly_cid_plaintext_t next_cid;
static int64_t start_time = 0;
static int64_t connect_time = 0;
static bool quit_after_first_byte = false;
static ptls_iovec_t resumption_token;

void client_timeout_cb(EV_P_ ev_timer *w, int revents);

void client_refresh_timeout()
{
    int64_t timeout = clamp_int64(quicly_get_first_timeout(conn) - client_ctx.now->cb(client_ctx.now),
                                  1, 200);
    client_timeout.repeat = timeout / 1000.;
    ev_timer_again(EV_DEFAULT, &client_timeout);
}

void client_timeout_cb(EV_P_ ev_timer *w, int revents)
{
    if(!send_pending(&client_ctx, client_socket, conn)) {
        quicly_free(conn);
        exit(0);
    }

    client_refresh_timeout();
}

void client_read_cb(EV_P_ ev_io *w, int revents)
{
    // retrieve data
    uint8_t buf[4096];
    struct sockaddr sa;
    socklen_t salen = sizeof(sa);
    quicly_decoded_packet_t packet;
    ssize_t bytes_received;

    while((bytes_received = recvfrom(w->fd, buf, sizeof(buf), MSG_DONTWAIT, &sa, &salen)) != -1) {
        for(ssize_t offset = 0; offset < bytes_received; ) {
            size_t packet_len = quicly_decode_packet(&client_ctx, &packet, buf, bytes_received, &offset);
            if(packet_len == SIZE_MAX) {
                break;
            }

            // handle packet --------------------------------------------------
            int ret = quicly_receive(conn, NULL, &sa, &packet);
            if(ret != 0 && ret != QUICLY_ERROR_PACKET_IGNORED) {
                fprintf(stderr, "quicly_receive returned %i\n", ret);
                exit(1);
            }

            // check if connection ready --------------------------------------
            if(connect_time == 0 && quicly_connection_is_ready(conn)) {
                connect_time = client_ctx.now->cb(client_ctx.now);
                int64_t establish_time = connect_time - start_time;
                printf("connection establishment time: %lums\n", establish_time);
            }
        }
    }

    if(errno != EWOULDBLOCK && errno != 0) {
        perror("recvfrom failed");
    }

    if(!send_pending(&client_ctx, client_socket, conn)) {
        quicly_free(conn);
        exit(0);
    }

    client_refresh_timeout();
}

void enqueue_request(quicly_conn_t *conn)
{
    quicly_stream_t *stream;
    int ret = quicly_open_stream(conn, &stream, 0);
    assert(ret == 0);
    const char *req = "qperf start sending";
    quicly_streambuf_egress_write(stream, req, strlen(req));
    quicly_streambuf_egress_shutdown(stream);
}

static void client_on_conn_close(quicly_closed_by_remote_t *self, quicly_conn_t *conn, int err,
                                 uint64_t frame_type, const char *reason, size_t reason_len)
{
    if (QUICLY_ERROR_IS_QUIC_TRANSPORT(err)) {
        fprintf(stderr, "transport close:code=0x%" PRIx16 ";frame=%" PRIu64 ";reason=%.*s\n", QUICLY_ERROR_GET_ERROR_CODE(err),
                frame_type, (int)reason_len, reason);
    } else if (QUICLY_ERROR_IS_QUIC_APPLICATION(err)) {
        fprintf(stderr, "application close:code=0x%" PRIx16 ";reason=%.*s\n", QUICLY_ERROR_GET_ERROR_CODE(err), (int)reason_len,
                reason);
    } else if (err == QUICLY_ERROR_RECEIVED_STATELESS_RESET) {
        fprintf(stderr, "stateless reset\n");
    } else {
        fprintf(stderr, "unexpected close:code=%d\n", err);
    }
}

static quicly_stream_open_t stream_open = {&client_on_stream_open};
static quicly_closed_by_remote_t closed_by_remote = {&client_on_conn_close};
static quicly_init_cc_t client_init_cc_reno = {&init_cc_reno};
static quicly_init_cc_t client_init_cc_cubic = {&init_cc_cubic};

int run_client(const char *port, bool gso, const char *logfile, const char *cc, const char *host, int runtime_s, bool ttfb_only)
{
    setup_session_cache(get_tlsctx());
    quicly_amend_ptls_context(get_tlsctx());

    client_ctx = quicly_spec_context;
    client_ctx.tls = get_tlsctx();
    client_ctx.stream_open = &stream_open;
    client_ctx.closed_by_remote = &closed_by_remote;
    client_ctx.transport_params.max_stream_data.uni = UINT32_MAX;
    client_ctx.transport_params.max_stream_data.bidi_local = UINT32_MAX;
    client_ctx.transport_params.max_stream_data.bidi_remote = UINT32_MAX;

    if(strcmp(cc, "reno") == 0) {
        client_ctx.init_cc = &client_init_cc_reno;
    } else if(strcmp(cc, "cubic") == 0) {
        client_ctx.init_cc = &client_init_cc_cubic;
    }

    if (gso) {
        enable_gso();
    }

    struct ev_loop *loop = EV_DEFAULT;

    struct sockaddr_storage sas;
    socklen_t salen;
    if(resolve_address((void*)&sas, &salen, host, port, AF_INET, SOCK_DGRAM, IPPROTO_UDP) != 0) {
        exit(-1);
    }

    struct sockaddr *sa = (void*)&sas;

    client_socket = socket(sa->sa_family, SOCK_DGRAM, IPPROTO_UDP);
    if(client_socket == -1) {
        perror("socket(2) failed");
        return 1;
    }

    struct sockaddr_in local = {0};
    local.sin_family = AF_INET;
    if (bind(client_socket, (void *)&local, sizeof(local)) != 0) {
        perror("bind(2) failed");
        return 1;
    }

    if (logfile)
    {
        setup_log_event(client_ctx.tls, logfile);
    }

    printf("starting client with host %s, port %s, runtime %is, cc %s\n", host, port, runtime_s, cc);
    quit_after_first_byte = ttfb_only;

    // start time
    start_time = client_ctx.now->cb(client_ctx.now);

    int ret = quicly_connect(&conn, &client_ctx, host, sa, NULL, &next_cid, resumption_token, 0, 0);
    assert(ret == 0);
    ++next_cid.master_id;

    enqueue_request(conn);
    if(!send_pending(&client_ctx, client_socket, conn)) {
        printf("failed to connect: send_pending failed\n");
        exit(1);
    }

    if(conn == NULL) {
        fprintf(stderr, "connection == NULL\n");
        exit(1);
    }

    ev_io socket_watcher;
    ev_io_init(&socket_watcher, &client_read_cb, client_socket, EV_READ);
    ev_io_start(loop, &socket_watcher);

    ev_init(&client_timeout, &client_timeout_cb);
    client_refresh_timeout();

    client_set_quit_after(runtime_s);

    ev_run(loop, 0);
    return 0;
}


void quit_client()
{
    if(conn == NULL) {
        return;
    }

    quicly_close(conn, 0, "");
    if(!send_pending(&client_ctx, client_socket, conn)) {
        printf("send_pending failed during connection close");
        quicly_free(conn);
        exit(0);
    }
    client_refresh_timeout();
}

void on_first_byte()
{
    printf("time to first byte: %lums\n", client_ctx.now->cb(client_ctx.now) - start_time);
    if(quit_after_first_byte) {
        quit_client();
    }
}
