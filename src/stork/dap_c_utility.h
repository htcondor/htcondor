#ifndef _DAP_C_UTILITY_H
#define  _DAP_C_UTILITY_H

#include <stdio.h>
#include <string.h>

char *strip_str(char *str);
void parse_url_with_port(char *url, char *protocol, char *host, char *port, char *filename);

#endif
