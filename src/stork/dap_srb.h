// Wrapper to include SRB client header file, with appropriate #defines, and C
// linkage.

// The following must be defined prior to including srbClient.h:
#if defined LINUX
#define PORTNAME_linux
#elif defined SOLARIS
#define PORTNAME_solaris
#else
#error Need to add new PORTNAME mapping here
#endif

#if defined(__cplusplus)
extern "C" {
#endif

#include "srbClient.h"

#if defined(__cplusplus)
}
#endif

