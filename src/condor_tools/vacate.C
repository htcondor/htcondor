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

/*********************************************************************
* Ask a machine which has been hosting a condor job to kick it off as
* though the machine had become busy with non-condor work.  Used
* for debugging...
*********************************************************************/


#include <stdio.h>
#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "condor_io.h"

static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)     */

extern "C" char *get_startd_addr(const char *);

usage( char *str )
{
	fprintf( stderr, "Usage: %s hostname-list\n", str );
	exit( 1 );
}

main( int argc, char *argv[] )
{
	int			cmd;
	int			i;
	char		*startdAddr;

	if( argc < 2 ) {
		usage( argv[0] );
	}

	config( 0 );

	if ((startdAddr = get_startd_addr(argv[1])) == NULL)
	{
		EXCEPT("Can't find startd address on %s\n", argv[1]);
	}

	for (i = 1; i < argc; i++) {

		    /* Connect to the specified host */
		ReliSock sock(startdAddr, START_PORT);
		if(sock.get_file_desc() < 0) {
			dprintf( D_ALWAYS, "Can't connect to condor startd (%s)\n",
					startdAddr );
			continue;
		}

		sock.encode();
		cmd = VACATE_ALL_CLAIMS;
		if (!sock.code(cmd) || !sock.eom()) {
			dprintf(D_ALWAYS, "Can't send VACATE_ALL_CLAIMS command to "
					"condor startd (%s)\n", startdAddr);
			continue;
		}
		
		printf( "Sent VACATE_ALL_CLAIMS command to startd on %s\n", argv[i] );

	}

	exit( 0 );
}

extern "C" SetSyscalls(){}
