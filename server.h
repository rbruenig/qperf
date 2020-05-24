#pragma once

#include <quicly.h>
#include <stdbool.h>

int run_server(const char* port, bool gso, const char *cert, const char *key);

