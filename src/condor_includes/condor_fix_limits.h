#ifndef FIX_LIMITS_H
#define FIX_LIMITS_H

#if defined(HPUX9)
#   define select _hide_select
#endif

#include <limits.h>

#if defined(HPUX9)
#   undef select
#endif

/* 
** Linux doesn't define WORD_BIT unless __SVR4_I386_ABI_L1__ is
** defined, and this is all we need, so lets just define it.
** OSF doesn't define it unless _XOPEN_SOURCE is defined...
** IRIX needs some other whacky thing defined...
*/ 
#if defined(LINUX) || defined(OSF1) || defined(IRIX53)
#   if !defined(WORD_BIT)
#		define WORD_BIT 32
#	endif 
#endif /* LINUX || OSF1 || IRIX */

#ifndef _POSIX_PATH_MAX
#define _POSIX_PATH_MAX 255
#endif

#ifndef _POSIX_ARG_MAX
#define _POSIX_ARG_MAX 4096
#endif

#endif /* FIX_LIMITS_H */
