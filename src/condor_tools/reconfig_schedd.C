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
* Tell the condor_schedd to re-read the configuration files and take
* appropriate actions.  This program is used by the Condor.throttle
* script to change the number of condor jobs a machine will "farm
* out" in parallel.
*********************************************************************/


#include "condor_common.h"
#include "condor_config.h"
#include "condor_io.h"
#include "my_hostname.h"
#include "get_full_hostname.h"

extern "C" char *get_schedd_addr(const char *);
void reconfig( char *host );

void usage( char *str )
{
	fprintf( stderr, "Usage: %s hostname\n", str );
	exit( 1 );
}

int main( int argc, char *argv[] )
{
	char* fullname, *MyName = argv[0];

	config( 0 );

	if( argc < 2 ) {
		reconfig( my_full_hostname() );
		exit( 0 );
	}

	for( argv++; *argv; argv++ ) {
		if( (fullname = get_full_hostname(*argv)) == NULL ) {
			fprintf( stderr, "%s: unknown host %s\n", MyName, *argv );
			continue;
		}
		reconfig( fullname );
	}
	exit( 0 );
}


void reconfig( char *host )
{
	int			cmd = RECONFIG;
	char		*scheddAddr;

	if ((scheddAddr = get_schedd_addr(host)) == NULL) {
		fprintf( stderr, "Can't find schedd address on %s\n", host );
		return;
	}

		/* Connect to the schedd */
	ReliSock sock(scheddAddr, SCHED_PORT);
	if(sock.get_file_desc() < 0) {
		fprintf( stderr, "Can't connect to condor scheduler (%s)\n",
				 scheddAddr );
		return;
	}

	sock.encode();
	if( !sock.code(cmd) || !sock.eom() ) {
		fprintf( stderr,
				 "Can't send RECONFIG command to condor scheduler\n" );
		return;
	}

	printf( "Sent RECONFIG command to schedd on %s\n", host );
}

extern "C" int SetSyscalls(){}
