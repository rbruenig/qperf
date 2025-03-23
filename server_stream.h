#pragma once

#include <quicly.h>

quicly_error_t server_on_stream_open(quicly_stream_open_t *self, quicly_stream_t *stream);
