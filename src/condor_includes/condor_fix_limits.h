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
*/ 
#if defined(LINUX)
#   if !defined(WORD_BIT)
#		define WORD_BIT 32
#	endif 
#endif /* LINUX */

#endif /* FIX_LIMITS_H */
