#include "condor_timer_manager.h"
#include "condor_io.h"

#include "stdsoap2.h"

void init_soap(struct soap *soap);

int handle_soap_ssl_socket(Service *, Stream *stream);

int http_get_handler(struct soap *soap);
