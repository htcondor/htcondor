#ifndef CONDOR_FIX_IOCTL_H
#define CONDOR_FIX_IOCTL_H

#if defined(Solaris)
#	define BSD_COMP
#endif

#include <sys/ioctl.h>

#if defined(Solaris)
#	undef BSD_COMP
#endif

#endif CONDOR_FIX_IOCTL_H
