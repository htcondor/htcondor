/* 
** Copyright 1986, 1987, 1988, 1989, 1990, 1991 by the Condor Design Team
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
** 
*/ 

#include <stdio.h>
#include <sys/file.h>
#include <sys/ioctl.h>

/* Solaris specific change ..dhaval 6/24 */
#if defined(Solaris)
#include <sys/fcntl.h>
#include <unistd.h>
#include <termios.h>
#endif

#include "debug.h"
#include "except.h"

#ifndef LINT
static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)     */
#endif LINT

detach()
{
	int		fd;

	(void)close( 0 );
	(void)close( 1 );
	(void)close( 2 );

#if !defined(HPUX9) && !defined(Solaris)
	if( (fd=open("/dev/tty",O_RDWR,0)) < 0 ) {
		dprintf( D_ALWAYS, "Can't open /dev/tty\n" );
		return;
	}
	if( ioctl(fd,TIOCNOTTY,(char *)0) < 0 ) {
		EXCEPT( "ioctl(%d,TIOCNOTTY,(char *)0)", fd );
	}

	(void)close( fd );
#endif
}
