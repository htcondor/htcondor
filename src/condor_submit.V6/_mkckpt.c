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
** Authors:  Allan Bricker and Michael J. Litzkow,
** 	         University of Wisconsin, Computer Sciences Dept.
** 
*/ 

#if defined(Solaris)
#include <sys/fcntl.h>
#endif

#include <sys/file.h>
#include "condor_sys.h"
#include "except.h"

#include <sys/types.h>
#include <netinet/in.h>

static char *_FileName_ = __FILE__;     /* Used by EXCEPT (see except.h)     */

_mkckpt( ckptfile, objfile )
char *ckptfile, *objfile;
{
	register int objfd, ckptfd;
	register int len = 0, cc;
	register int i;
	int ssize;
	char buf[ 4 * 1024 ];

	/*
	**	Make sure to do local calls...
	*/
	(void) SetSyscalls( SYS_LOCAL | SYS_RECORDED );

	objfd = open( objfile, O_RDONLY, 0 );
	if( objfd < 0 ) {
		EXCEPT("open %s", objfile);
	}

	ckptfd = open( ckptfile, O_CREAT|O_TRUNC|O_WRONLY, 0777);
	if( ckptfd < 0 ) {
		EXCEPT("open %s", ckptfile);
	}

	for(;;) {
		cc = read(objfd, buf, sizeof(buf));
		if( cc < 0 ) {
			EXCEPT("read %s: len = %d", objfile, len);
		}

		if( write(ckptfd, buf, cc) != cc ) {
			EXCEPT("write %s: cc = %d, len = %d", ckptfile, cc, len);
		}

		len += cc;

		if( cc != sizeof(buf) ) {
			break;
		}
	}

	if( fsync(ckptfd) < 0 ) {
		EXCEPT( "Write of \"%s\"", ckptfile );
	}

	(void)close( objfd );
	(void)close( ckptfd );
}
