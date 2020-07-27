#pragma once

#include <quicly.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/syscall.h>

ptls_context_t *get_tlsctx();

struct addrinfo *get_address(const char *host, const char *port);
void enable_gso();
bool send_pending(quicly_context_t *ctx, int fd, quicly_conn_t *conn);
void print_escaped(const char *src, size_t len);


static inline int64_t min_int64(int64_t a, int64_t b)
{
    if(a < b) {
        return a;
    } else {
        return b;
    }
}

static inline int64_t max_int64(int64_t a, int64_t b) {
    if(a > b) {
        return a;
    } else {
        return b;
    }
}

static inline int64_t clamp_int64(int64_t val, int64_t min, int64_t max)
{
    if(val < min) {
        return min;
    }
    if(val > max) {
        return max;
    }
    return val;
}

static inline uint64_t get_current_pid()
{
    uint64_t pid;

    #ifdef __APPLE__
        pthread_threadid_np(NULL, &pid);
    #else
        pid = syscall(SYS_gettid);
    #endif

    return pid;
}

static void init_cc_reno(quicly_init_cc_t *init_cc, quicly_cc_t *cc, uint32_t initcwnd, int64_t now)
{
    quicly_cc_reno_init(cc, initcwnd);
}

static void init_cc_cubic(quicly_init_cc_t *init_cc, quicly_cc_t *cc, uint32_t initcwnd, int64_t now)
{
    quicly_cc_cubic_init(cc, initcwnd);
}