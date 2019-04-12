#pragma once

#include <quicly.h>

int server_on_stream_open(quicly_stream_open_t *self, quicly_stream_t *stream);
