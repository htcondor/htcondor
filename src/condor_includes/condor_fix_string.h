#ifndef FIX_STRING_H
#define FIX_STRING_H

/*
  Some versions of string.h now include the function strdup(), which is
  functionally equivalent to the one in the condor_util_lib directory.
  This would be OK, if they had remembered to declare the single
  parameter as a "const" as is obviously intended.  Since they forgot,
  we have to hide that definition here and provide our own.  -- mike
*/

#define strdup _hide_strdup
#include <string.h>
#undef strdup

#if defined(__cplusplus)
extern "C" {
#endif

char *strdup( const char * );

#if defined(__cplusplus)
}
#endif

#endif

