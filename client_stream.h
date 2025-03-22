#pragma once

#include <quicly.h>

quicly_error_t client_on_stream_open(quicly_stream_open_t *self, quicly_stream_t *stream);
void client_set_quit_after(int seconds);
