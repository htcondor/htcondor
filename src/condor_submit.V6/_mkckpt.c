/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

 

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
