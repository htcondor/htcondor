#if defined(WIN32)

#define NOGDI
#define NOUSER
#define NOSOUND
#include <winsock2.h>
#include <windows.h>
#include "_condor_fix_nt.h"
#include <stdlib.h>
#include <stdio.h>

#else

#include "_condor_fix_types.h"
#include "condor_fix_stdio.h"
#include <stdlib.h>
#include "condor_fix_unistd.h"
#include "condor_fix_limits.h"
#include "condor_fix_string.h"
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include "condor_fix_signal.h"

#if defined(Solaris)
#	define BSD_COMP
#endif
#include <sys/ioctl.h>
#if defined(Solaris)
#	undef BSD_COMP
#endif

#endif // defined(WIN32)
