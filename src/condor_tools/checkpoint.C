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
** Authors:  Allan Bricker and Michael J. Litzkow, Todd Tannenbaum,
** 	         University of Wisconsin, Computer Sciences Dept.
** 
*/ 

/*********************************************************************
* Ask a machine which has been hosting a condor job to perform a 
* periodic checkpoint of the job.
*********************************************************************/

#include "condor_common.h"
#include "condor_config.h"
#include "condor_io.h"
#include "get_full_hostname.h"

extern "C" char *get_startd_addr(const char *);

usage( char *str )
{
	fprintf( stderr, "Usage: %s hostname [other hostnames]\n", str );
	exit( 1 );
}

main( int argc, char *argv[] )
{
	int			cmd = PCKPT_ALL_JOBS;
	int			i;
	char		*startdAddr;
	char		*fullname;

	if( argc < 2 ) {
		usage( argv[0] );
	}

	config( 0 );

#if !defined(WIN32)
	install_sig_handler(SIGPIPE, SIG_IGN );
#endif

	for (i = 1; i < argc; i++) {

		if( (fullname = get_full_hostname(argv[i])) == NULL ) {
			fprintf( stderr, "%s: unknown host %s\n", argv[0], argv[i] );
			continue;
		}
		if( (startdAddr = get_startd_addr(fullname)) == NULL ) {
			fprintf( stderr, "%s: can't find address of startd on %s\n", 
					 argv[0], argv[i] );
			continue;
		}

		    /* Connect to the specified host */
		ReliSock sock(startdAddr, START_PORT);
		if(sock.get_file_desc() < 0) {
			fprintf( stderr, "Can't connect to condor startd (%s)\n",
					startdAddr );
			continue;
		}

		sock.encode();
		if (!sock.code(cmd) || !sock.eom()) {
			fprintf( stderr, "Can't send PCKPT_ALL_JOBS command to "
					 "condor startd (%s)\n", startdAddr);
			continue;
		}
		
		printf( "Sent PCKPT_ALL_JOBS command to startd on %s\n", argv[i] );

	}

	exit( 0 );
}

extern "C" SetSyscalls(){}
