/* 
** Copyright 1993 by Miron Livny, and Mike Litzkow
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the copyright holders not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the 
** copyright holders make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE COPYRIGHT HOLDERS DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Author:  Mike Litzkow
**
*/ 

#include "condor_debug.h"
static char *_FileName_ = __FILE__;

/*
  Tell the operating system that we want to operate in "POSIX
  conforming mode".  Isn't it ironic that such a request is itself
  non-POSIX conforming?
*/
#if defined(ULTRIX42) || defined(ULTRIX43)
#include <sys/sysinfo.h>
#define A_POSIX 2		// from exec.h, which I prefer not to include here...
extern "C" {
	int setsysinfo( unsigned, char *, unsigned, unsigned, unsigned );
}
void
set_posix_environment()
{
	int		name_value[2];

	name_value[0] = SSIN_PROG_ENV;
	name_value[1] = A_POSIX;

	if( setsysinfo(SSI_NVPAIRS,(char *)name_value,1,0,0) < 0 ) {
		EXCEPT( "setsysinfo" );
	}
}
#else
void
set_posix_environment() {}
#endif
