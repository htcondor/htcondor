#ifdef HAVE_EXT_GSOAP
#include "stdsoap2.h"
#else
#include <stddef.h>
struct soap;
#define SOAP_FMAC1
#define SOAP_FMAC2
#endif

SOAP_FMAC1 const char** SOAP_FMAC2 soap_faultcode(struct soap*)
{ return NULL; }
SOAP_FMAC1 const char** SOAP_FMAC2 soap_faultsubcode(struct soap*)
{ return NULL; }
SOAP_FMAC1 const char** SOAP_FMAC2 soap_faultstring(struct soap*)
{ return NULL; }
SOAP_FMAC1 const char** SOAP_FMAC2 soap_faultdetail(struct soap*)
{ return NULL; }
SOAP_FMAC1 void SOAP_FMAC2 soap_serializeheader(struct soap*)
{}
SOAP_FMAC1 int SOAP_FMAC2 soap_putheader(struct soap*)
{ return 0; }
SOAP_FMAC1 int SOAP_FMAC2 soap_getheader(struct soap*)
{ return 0; }
SOAP_FMAC1 void SOAP_FMAC2 soap_serializefault(struct soap*)
{}
SOAP_FMAC1 int SOAP_FMAC2 soap_putfault(struct soap*)
{ return 0; }
SOAP_FMAC1 int SOAP_FMAC2 soap_getfault(struct soap*)
{ return 0; }