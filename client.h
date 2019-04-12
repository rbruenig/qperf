#pragma once

#include <stdbool.h>
#include <stdint.h>

int run_client(const char *host, int runtime_s, bool ttfb_only);
void quit_client();
void quit_client();

void on_first_byte();
