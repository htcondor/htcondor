#ifndef CONDOR_FIX_NETDB_H
#define CONDOR_FIX_NETDB_H

#if defined(OSF1) 
#	define gethostid _hide_gethostid
#endif

#include <netdb.h>

#if defined(OSF1) 
#	undef gethostid
#endif

#endif /* CONDOR_FIX_NETDB_H */
