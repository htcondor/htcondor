#include "condor_config.h"

/* Define a version of lower_case which is happy on various platforms. */

#ifndef _tolower
#define _tolower(c) ((c) + 'a' - 'A')
#endif
/*
** Transform the given string into lower case in place.
*/
void
lower_case( register char   *str )
{
#if defined(AIX32)
#   define ANSI_STYLE 0
#else
#   define ANSI_STYLE 1
#endif

    for( ; *str; str++ ) {
        if( *str >= 'A' && *str <= 'Z' )
#if ANSI_STYLE
            *str = _tolower( (int)(*str) );
#else
            *str |= 040;
#endif
    }
}



