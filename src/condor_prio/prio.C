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

#define _POSIX_SOURCE

#include "condor_common.h"
#include "condor_constants.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "proc_obj.h"
#include "filter.h"
#include "alloc.h"

static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)     */

DBM		*Q;

char	*param();
char	*MyName;
char	*Spool;

int		PrioAdjustment;
int		NewPriority;
int		PrioritySet;
int		AdjustmentSet;
ProcFilter	*PFilter = new ProcFilter();
int		InvokingUid;		// user id of person ivoking this program
char	*InvokingUserName;	// user name of person ivoking this program

const int MIN_PRIO = -20;
const int MAX_PRIO = 20;

	// Prototypes of local interest only
void usage();
int compute_adj( char * );
void init_params();
void do_it( ProcObj * );
int calc_prio( int old_prio );
void init_user_credentials();

	// Tell folks how to use this program
void
usage()
{
	fprintf( stderr, "Usage: %s [{+|-}priority ] [-p priority] ", MyName );
	fprintf( stderr, "[ -a ] [user | cluster | cluster.proc] ...\n" );
	exit( 1 );
}

int
main( int argc, char *argv[] )
{
	char	queue_name[_POSIX_PATH_MAX];
	char	*arg;
	int		prio_adj;
	List<ProcObj>	*proc_list;
	ProcObj			*p;

	MyName = argv[0];

	config( MyName, (CONTEXT *)0 );
	init_params();

	if( argc < 2 ) {
		usage();
	}

	PrioritySet = 0;
	AdjustmentSet = 0;

	for( argv++; arg = *argv; argv++ ) {
		if( (arg[0] == '-' || arg[0] == '+') && isdigit(arg[1]) ) {
			PrioAdjustment = compute_adj(arg);
			AdjustmentSet = TRUE;
		} else if( arg[0] == '-' && arg[1] == 'p' ) {
			argv++;
			NewPriority = atoi(*argv);
			PrioritySet = TRUE;
		} else {
			if( !PFilter->Append(arg) ) {
				usage();
			}
		}
	}

	if( PrioritySet == FALSE && AdjustmentSet == FALSE ) {
		fprintf( stderr, 
			"You must specify a new priority or priority adjustment.\n");
		usage();
		exit(1);
	}

	if( PrioritySet && (NewPriority < MIN_PRIO || NewPriority > MAX_PRIO) ) {
		fprintf( stderr, 
			"Invalid priority specified.  Must be between %d and %d.\n", 
			MIN_PRIO, MAX_PRIO );
		exit(1);
	}

		/* Open job queue */
	(void)sprintf( queue_name, "%s/job_queue", Spool );
	if( (Q=OpenJobQueue(queue_name,O_RDWR,0)) == NULL ) {
		EXCEPT( "OpenJobQueue(%s)", queue_name );
	}

	LockJobQueue( Q, WRITER );


		// Create a list of all procs we're interested in
	PFilter->Install();
	proc_list = ProcObj::create_list( Q, ProcFilter::Func );

		// Process the list
	proc_list->Rewind();
	while( p = proc_list->Next() ) {
		do_it( p );
	}

		// Clean up
	LockJobQueue( Q, UNLOCK );
	CloseJobQueue( Q );
	ProcObj::delete_list( proc_list );
	delete PFilter;

#if defined(ALLOC_DEBUG)
	print_alloc_stats();
#endif

	exit( 0 );
}

/*
  Given the old priority of a given process, calculate the new
  one based on information gotten from the command line arguments.
*/
int
calc_prio( int old_prio )
{
	int		answer;

	if( AdjustmentSet == TRUE && PrioritySet == FALSE ) {
		answer = old_prio + PrioAdjustment;
		if( answer > MAX_PRIO )
			answer = MAX_PRIO;
		else if( answer < MIN_PRIO )
			answer = MIN_PRIO;
	} else {
		answer = NewPriority;
	}
	return answer;
}

/*
  Given a command line argument specifing a relative adjustment
  of priority of the form "+adj" or "-adj", return the correct
  value as a positive or negative integer.
*/
int
compute_adj( char *arg )
{
	char *ptr;
	int val;
	
	ptr = arg+1;

	val = atoi(ptr);

	if( *arg == '-' ) {
		return( -val );
	} else {
		return( val );
	}
}

extern "C" int SetSyscalls( int foo ) { return foo; }

void
init_params()
{
	Spool = param("SPOOL");
	if( Spool == NULL ) {
		EXCEPT( "SPOOL not specified in config file\n" );
	}
}

void
do_it( ProcObj *p )
{
	int		old_prio;
	int		new_prio;

	if( p->perm_to_modify() ) {
		old_prio = p->get_prio();
		new_prio = calc_prio( old_prio );
		p->set_prio( new_prio );
		p->store( Q );
	} else {
		fprintf( stderr,
			"%d.%d: Permission Denied\n",
			p->get_cluster_id(), p->get_proc_id()
		);
	}
}
