#ifndef _DAP_UTILITY_H
#define  _DAP_UTILITY_H

#include <stdio.h>
#include <string.h>

void parse_url(char *url, char *protocol, char *host, char *filename);
char *strip_str(char *str);

#endif
