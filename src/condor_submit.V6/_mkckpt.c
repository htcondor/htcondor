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
** Modified by Jim Basney to use qmgmt services to transfer initial
**   checkpoint file via Shadow.  11/5/96
*/ 

#if defined(Solaris)
#include <sys/fcntl.h>
#endif

#include <sys/file.h>
#include "condor_sys.h"
#include "condor_fix_stdio.h"
#include "except.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>


#include "condor_qmgr.h"
#include "_condor_fix_types.h"
#include "condor_fix_socket.h"

extern char *Spool;

static char *_FileName_ = __FILE__;     /* Used by EXCEPT (see except.h)     */

_mkckpt( ckptfile, objfile )
char *ckptfile, *objfile;
{
	register int objfd, ckptfd;
	register int len = 0, cc;
	register int i;
	int ssize, ack;
	char buf[ 4 * 1024 ];
	char address[256];
	struct sockaddr_in sin;
	struct stat filesize;

	/*
	**	Make sure to do local calls...
	*/
	(void) SetSyscalls( SYS_LOCAL | SYS_RECORDED );

	objfd = open( objfile, O_RDONLY, 0 );
	if( objfd < 0 ) {
		EXCEPT("open %s", objfile);
	}

	if (SendSpoolFile(ckptfile, address) < 0) {
		EXCEPT("unable to transfer checkpoint file %s", ckptfile);
	}

	ckptfd = socket( AF_INET, SOCK_STREAM, 0 );
	if (ckptfd < 0 ) {
		EXCEPT("socket call failed");
	}
	
	memset( &sin, '\0', sizeof sin );
	sin.sin_family = AF_INET;
	if (string_to_sin(address, &sin) < 0) {
		EXCEPT("unable to decode address %s", address);
	}

	if (connect(ckptfd, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		EXCEPT("failed to connect to qmgmr to transfer checkpoint file");
	}

	if (fstat(objfd, &filesize) < 0) {
		EXCEPT("fstat of executable %s failed", objfile);
	}

	filesize.st_size = htonl(filesize.st_size);
	if ( write(ckptfd, &filesize.st_size, sizeof(int)) < 0 ) {
		EXCEPT("filesize write failed");
	}

	for(;;) {
		cc = read(objfd, buf, sizeof(buf));
		if( cc < 0 ) {
			EXCEPT("read %s: len = %d", objfile, len);
		}

		if( write(ckptfd, buf, cc) != cc ) {
			fprintf(stderr,"Error writing initial executable into queue\nPerhaps no more space available in directory %s ?\n",Spool);
			EXCEPT("write %s: cc = %d, len = %d", ckptfile, cc, len);
		}

		len += cc;

		if( cc != sizeof(buf) ) {
			break;
		}
	}

	if ( read(ckptfd, &ack, sizeof(ack)) < 0) {
		EXCEPT("Failed to read ack from qmgmr!  Checkpoint store failed!");
	}

	ack = ntohl(ack);
	if (ack != len) {
		EXCEPT("Failed to transfer %d bytes of checkpoint file (only %d)",
			   len, ack);
	}

	(void)close( objfd );
	(void)close( ckptfd );
}
