#ifndef CONDOR_RESOURCE_H
#define CONDOR_RESOURCE_H

#if defined(__cplusplus)
extern "C" {
#endif

#if ( defined(HPUX9) && !defined(HPUX10) )
	typedef int rlim_t;
#endif

#if !defined(WIN32)
#include <sys/resource.h>
#endif

#if defined(__cplusplus)
}
#endif

#endif /* CONDOR_RESOURCE_h */
