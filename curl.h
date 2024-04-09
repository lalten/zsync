#pragma once

#include <stdio.h>

FILE *curl_open(const char **restrict curl_options);
int curl_close(FILE *stream);
