#ifndef FIX_LIMITS_H
#define FIX_LIMITS_H

#if defined(HPUX9)
#   define select _hide_select
#endif

#include <limits.h>

#if defined(HPUX9)
#   undef select
#endif

#endif
