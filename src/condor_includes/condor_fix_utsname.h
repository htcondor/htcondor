#ifndef _UTSNAME_H
#define _UTSNAME_H

#if defined( __cplusplus )
extern "C" {
#endif

/*
  The Sun include file only provides a non-ANSI prototype for the
  uname() function, so we have to cover it up here.
*/
#if defined(SUNOS41) && ( defined(__STDC__) || defined(__cplusplus) )
#define uname hide_uname
#endif

#include <sys/utsname.h>

/*
  Here we proivde the ANSI style prototype for the uname() function
  which SUN omitted.
*/
#if defined(SUNOS41) && ( defined(__STDC__) || defined(__cplusplus) )
#undef uname
int uname( struct utsname * );
#endif

#if defined( __cplusplus )
}
#endif

#endif
