/*
  NOTE: we do *NOT* want to include "condor_common.h" and all the
  associated header files for this, since it's a very simple function
  and we want to be able to build it in a stand-alone version of the
  UserLog parser for libcondorapi.a
*/

/* Define a version of lower_case which is happy on various platforms. */

#ifndef _tolower
#define _tolower(c) ((c) + 'a' - 'A')
#endif

extern "C" {

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


} /* extern "C" */

