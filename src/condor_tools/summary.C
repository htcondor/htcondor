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

/****************************************************************
* Read through the history of all the condor jobs that have ever been
* submitted on this machine and print a summary of that information.
* This could take a long time...
****************************************************************/

#define _POSIX_SOURCE

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_constants.h"
#include "condor_config.h"
#include "condor_jobqueue.h"
#include "proc_obj.h"
#include "alloc.h"

static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)     */
extern "C" char * format_time ( float fp_secs );

class UserRec {
public:
	UserRec( const char *owner );
	~UserRec();
	char	*get_name()			{ return name; }
	int		get_n_jobs()		{ return n_jobs; }
	float	get_local_cpu()		{ return local_cpu; }
	float	get_remote_cpu()	{ return remote_cpu; }
	void	update( float, float );
	void	display();
	static void	display_header();
private:
	char	*name;
	int		n_jobs;
	float	local_cpu;
	float	remote_cpu;
};

List<UserRec>	*UserList;
UserRec			*Totals = new UserRec( "TOTAL" );

char		*MyName;	// Name by which this program was invoked
char		*Spool;		// Name of job queue directory
BOOLEAN		PrintTotals = TRUE;	// Whether or not to print totals for all users
ProcFilter *PFilter = new ProcFilter();
XDR		xdr, *H = &xdr;	// XDR stream connected to the history file


	// Prototypes of local interest
void init_params();
void usage();
void process( ProcObj *p );
UserRec *find_user( const char *user );

/*
  Tell folks how to use this program.
*/
void
usage()
{
	fprintf( stderr,
		"Usage: %s [-f file] [cluster | cluster.proc | user_name ] ...\n",
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
	UserRec	*u;

	config( *argv, (CONTEXT *)0 );
	init_params();

	for( MyName = *argv++; arg = *argv; argv++ ) {
		if( arg[0] == '-' && arg[1] == 'f' ) {
			special_hist_file = *(++argv);
		} else if( arg[0] == '-' ) {
			usage();
		} else {
			if( !PFilter->Append(arg) ) {
				usage();
			}
			PrintTotals = FALSE;
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


		// Create a list of all procs we're interested in
	PFilter->Install();
	proc_list = ProcObj::create_list( H, ProcFilter::Func );

		// Process the list
	UserList = new List<UserRec>;
	proc_list->Rewind();
	while( p = proc_list->Next() ) {
		process( p );
	}

		// Display the results
	UserRec::display_header();
	UserList->Rewind();
	while( u = UserList->Next() ) {
		u->display();
	}
	if( PrintTotals ) {
		printf( "\n" );
		Totals->display();
	}

		// Cleanup
	(void)LockHistory( H, UNLOCK );
	(void)CloseHistory( H );
	ProcObj::delete_list( proc_list );
	delete PFilter;

	UserList->Rewind();
	while( u = UserList->Next() ) {
		delete u;
	}
	delete UserList;
	delete Totals;

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

#if 0
display_list()
{
	int			i;
	USER_REC	*rec;

	printf( "%8.8s %6.6s %14.14s %14.14s   %12.12s\n",
		"Name", "Jobs", "Local Cpu", "Remote Cpu", "Leverage" );

	for( i=0; i<N_Recs; i++ ) {
		rec = RecList[i];
		print_rec( rec->name, rec->n_jobs, rec->local_cpu, rec->remote_cpu );
	}
	print_rec( "TOTAL", TotalJobs, TotalLocalCpu, TotalRemoteCpu );
}

print_rec( name, n_jobs, local, remote )
char	*name;
int		n_jobs;
float	local;
float	remote;
{

	printf( "%8.8s %6d ", name, n_jobs );
	printf( "%14s ", format_time(local) );
	printf( "%14s   ", format_time(remote) );
	if( local < 1.0 ) {
		printf( "%12.12s\n", "(undefined)" );
	} else {
		printf( "%12.1f\n", remote / local );
	}
}
#endif

void
process( ProcObj *p )
{
	char	*owner;
	UserRec	*user_rec;

	owner = p->get_owner();
	user_rec = find_user( owner );
	if( !user_rec ) {
		user_rec = new UserRec(owner);
		UserList->Append( user_rec );
	};
	// user_rec->update( p->get_local_cpu(), p->get_remote_cpu() );
	user_rec->update( 0.0, 0.0 );
	Totals->update( 0.0, 0.0 );
}

UserRec *
find_user( const char *user )
{
	UserRec	*cur;
	UserList->Rewind();
	while( cur = UserList->Next() ) {
		if( strcmp(cur->get_name(),user) == MATCH ) {
			return cur;
		}
	}
	return 0;
}


UserRec::UserRec( const char *owner )
{
	name = string_copy( owner );
	n_jobs = 0;
	local_cpu = 0.0;
	remote_cpu = 0.0;
}

UserRec::~UserRec()
{
	delete [] name;
}

void
UserRec::update( float local, float remote )
{
	n_jobs += 1;
	local_cpu += local;
	remote_cpu += remote;
}

void
UserRec::display()
{
	printf( "%8.8s %6d ", name, n_jobs );
	printf( "%14s ", format_time(local_cpu) );
	printf( "%14s   ", format_time(remote_cpu) );
	if( local_cpu < 1.0 ) {
		printf( "%12.12s\n", "(undefined)" );
	} else {
		printf( "%12.1f\n", remote_cpu / local_cpu );
	}
}

void
UserRec::display_header()
{
	printf( "%8.8s %6.6s %14.14s %14.14s   %12.12s\n",
		"Name", "Jobs", "Local Cpu", "Remote Cpu", "Leverage" );
}
