/* 
** Copyright 1986, 1987, 1988, 1989, 1990, 1991, 1992 by the Condor Design Team
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the Condor Design Team not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the Condor
** Design Team make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE CONDOR DESIGN TEAM DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE CONDOR DESIGN TEAM BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Author:  Michael J. Litzkow
** 	         University of Wisconsin, Computer Sciences Dept.
** Cleaned up on 12/17/97 by Derek Wright to deal with platforms that
**   don't know about _NFILE, etc.
** 
*/ 

#include "condor_common.h"

#ifdef OPEN_MAX
static int open_max = OPEN_MAX;
#else
static int open_max = 0;
#endif
#define OPEN_MAX_GUESS 256

/*
** Compatibility routine for systems which don't have this call.
*/
int
getdtablesize()
{
	if( open_max == 0 ) {	/* first time */
		errno = 0;
		if( (open_max = sysconf(_SC_OPEN_MAX)) < 0 ) {
			if( errno == 0 ) {
					/* _SC_OPEN_MAX is indeterminate */
				open_max = OPEN_MAX_GUESS;
			} else {
					/* Error in sysconf */
			}
		}	
	}
	return open_max;
}
