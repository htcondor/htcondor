#ifndef FIX_WAIT_H
#define FIX_WAIT_H

/* To get union wait on OSF1, _OSF_SOURCE & _BSD must be defined */

#if defined(OSF1)
#if defined(_OSF_SOURCE)
#define _TMP_OSF_SOURCE
#else
#define _OSF_SOURCE
#endif

#if defined(_BSD)
#define _TMP_BSD
#else
#define _BSD
#endif
#endif

#endif /* OSF1 */

#include <sys/wait.h>

#if defined(OSF1)

#if !defined(_TMP_OSF_SOURCE)
#undef _OSF_SOURCE
#endif
#undef _TMP_OSF_SOURCE

#if !defined(_TMP_BSD)
#undef _BSD
#endif
#undef _TMP_BSD

#endif /* OSF1 */
