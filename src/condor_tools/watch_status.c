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
** Authors:  Michael J. Litzkow,
** 	         University of Wisconsin, Computer Sciences Dept.
** 
*/ 

/*********************************************************************
* 
* Monitor status of Condor on local machine.
* 
*********************************************************************/


#include <stdio.h>
#include "sched.h"
#include "debug.h"
#include "except.h"
#include "expr.h"

static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)     */

char	*MyName;
int		Interval = 1;

usage( name )
char	*name;
{
	fprintf( stderr, "Usage: %s [seconds]\n", name );
	exit( 1 );
}

main( argc, argv )
int		argc;
char	*argv[];
{
	MyName = argv[0];
	config( MyName, (CONTEXT *)0 );

	if( argc > 2 ) {
		usage( argv[0] );
	}
	argv++;
	if( *argv ) {
		Interval = atoi( *argv );
	}

	for(;;) {
		print_status();
		sleep( Interval );
	}
}

print_status()
{
	int		status;
	char	buf[81];
	char	*msg;

	switch( status = get_machine_status() ) {
	  case JOB_RUNNING:
		msg = "JOB RUNNING";
		break;
	  case KILLED:
		msg = "JOB EXITING";
		break;
	  case CHECKPOINTING:
		msg = "JOB CHECKPOINTING";
		break;
	  case SUSPENDED:
		msg = "JOB SUSPENDED";
		break;
	  case USER_BUSY:
		msg = "USER BUSY";
		break;
	  case M_IDLE:
		msg = "IDLE";
		break;
	  case CONDOR_DOWN:
		msg = "CONDOR_DOWN";
		break;
	  case BLOCKED:
		msg = "BLOCKED";
		break;
	  case SYSTEM:
		msg = "SYSTEM";
		break;
	  default:
		sprintf( buf, "UNKNOWN (%d)", status );
		msg = buf;
		break;
	}

	printf( "\r%-20s", msg );
	fflush( stdout );
}

SetSyscalls(){}
