#include "client_stream.h"
#include "common.h"
#include "client.h"

#include <ev.h>
#include <stdbool.h>
#include <quicly/streambuf.h>

static int current_second = 0;
static uint64_t bytes_received = 0;
static ev_timer report_timer;
static bool first_receive = true;
static int runtime_s = 10;


void format_size(char *dst, double bytes)
{
    bytes *= 8;
    const char *suffixes[] = {"bit/s", "kbit/s", "mbit/s", "gbit/s"};
    int i = 0;
    while(i < 4 && bytes > 1024) {
        bytes /= 1024;
        i++;
    }
    sprintf(dst, "%.4g %s", bytes, suffixes[i]);
}

static void report_cb(EV_P_ ev_timer *w, int revents)
{
    char size_str[100];
    format_size(size_str, bytes_received);

    printf("second %i: %s (%lu bytes received)\n", current_second, size_str, bytes_received);
    fflush(stdout);
    ++current_second;
    bytes_received = 0;

    if(current_second >= runtime_s) {
        quit_client();
    }
}

static void client_stream_send_stop(quicly_stream_t *stream, int err)
{
    fprintf(stderr, "received STOP_SENDING: %i\n", err);
}

static void client_stream_receive(quicly_stream_t *stream, size_t off, const void *src, size_t len)
{
    if(first_receive) {
        bytes_received = 0;
        first_receive = false;
        ev_timer_init(&report_timer, report_cb, 1.0, 1.0);
        ev_timer_start(ev_default_loop(0), &report_timer);
        on_first_byte();
    }

    if(len == 0) {
        return;
    }

    bytes_received += len;
    quicly_stream_sync_recvbuf(stream, len);
}

static void client_stream_receive_reset(quicly_stream_t *stream, int err)
{
    fprintf(stderr, "received RESET_STREAM: %i\n", err);
}

static const quicly_stream_callbacks_t client_stream_callbacks = {
    &quicly_streambuf_destroy,
    &quicly_streambuf_egress_shift,
    &quicly_streambuf_egress_emit,
    &client_stream_send_stop,
    &client_stream_receive,
    &client_stream_receive_reset
};


int client_on_stream_open(quicly_stream_open_t *self, quicly_stream_t *stream)
{
    int ret = quicly_streambuf_create(stream, sizeof(quicly_streambuf_t));
    assert(ret == 0);
    stream->callbacks = &client_stream_callbacks;

    return 0;
}


void client_set_quit_after(int seconds)
{
    runtime_s = seconds;
}
