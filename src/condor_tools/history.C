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

/********************************************************************
* Print a summary of each job that has been submitted to condor in the
* history of this machine.
********************************************************************/


#define _POSIX_SOURCE
#if 0
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include "types.h"
#include "util_lib_proto.h"
#include "proc_obj.h"
#include "filter.h"
#include "alloc.h"
#else
#include "condor_common.h"
#include "condor_debug.h"
#include "condor_constants.h"
#include "condor_config.h"
#include "condor_jobqueue.h"
#include "proc_obj.h"
#include "alloc.h"
#endif

static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)     */

char		*MyName;	// Name by which this program was invoked
char		*Spool;		// Name of job queue directory
BOOLEAN		Long;		// Whether or not to use long display format.

ProcFilter *PFilter = new ProcFilter();
XDR		xdr, *H = &xdr;	// XDR stream connected to the history file

	// Prototypes of local interest
void init_params();
void usage();

/*
  Tell folks how to use this program.
*/
void
usage()
{
	fprintf( stderr,
		"Usage: %s [-l] [-f file] [cluster | cluster.proc | user_name ] ...\n",
		MyName
	);
	exit( 1 );
}

int
main( int argc, char *argv[] )
{
	char	history_name[_POSIX_PATH_MAX];
	char	*special_hist_file = NULL;
	char	*arg;
	List<ProcObj>	*proc_list;
	ProcObj			*p;
	int		dummy;

	config( *argv, (CONTEXT *)0 );
	init_params();

	for( MyName = *argv++; arg = *argv; argv++ ) {
		if( arg[0] == '-' && arg[1] == 'l' ) {
			Long = TRUE;
		} else if( arg[0] == '-' && arg[1] == 'f' ) {
			special_hist_file = *(++argv);
		} else if( arg[0] == '-' ) {
			usage();
		} else {
			if( !PFilter->Append(arg) ) {
				usage();
			}
		}
	}

		// Open history file
	if( special_hist_file ) {
		(void)strcpy( history_name, special_hist_file );
	} else {
		(void)sprintf( history_name, "%s/history", Spool );
	}

	H = OpenHistory( history_name, H, &dummy );
	(void)LockHistory( H, READER );


	if( !Long ) {
		ProcObj::short_header();
	}

		// Create a list of all procs we're interested in
	PFilter->Install();
	proc_list = ProcObj::create_list( H, ProcFilter::Func );

		// Display the list
	proc_list->Rewind();
	while( p = proc_list->Next() ) {
		if( Long ) {
			p->display();
		} else {
			p->display_short();
		}
	}

		// Cleanup
	(void)LockHistory( H, UNLOCK );
	(void)CloseHistory( H );
	ProcObj::delete_list( proc_list );
	delete PFilter;

#if defined(ALLOC_DEBUG)
	print_alloc_stats();
#endif

	exit( 0 );

}

/*
  Find out where the history file is stored on this machine by looking
  into configuration files.
*/
void
init_params()
{
	char	*param();

	Spool = param("SPOOL");
	if( Spool == NULL ) {
		EXCEPT( "SPOOL not specified in config file\n" );
	}
}

extern "C" int
SetSyscalls( int foo ) { return foo; }
