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


/*********************************************************************
* Find files which may have been left lying around on somebody's workstation
* by condor.  Depending on command line options we may remove the file
* and/or mail appropriate authorities about it.
*********************************************************************/


#define _POSIX_SOURCE

#include <time.h>
#include "condor_common.h"
#include <sys/stat.h>
#include "condor_debug.h"
#include "condor_constants.h"
#include "condor_config.h"
#include "condor_jobqueue.h"
#include "condor_mach_status.h"
#include "condor_uid.h"
#include "proc_obj.h"
#include "directory.h"
#include "alloc.h"

#if defined(OSF1)
#pragma define_template List<char>
#pragma define_template Item<char>
#endif

char *my_hostname();

static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)     */

	// Define this to check for memory leaks

int			MaxCkptInterval;	// max time between ckpts on this machine
char		*Spool;				// dir for condor job queue
char		*Execute;			// dir for execution of condor jobs
char		*Log;				// dir for condor program logs
char		*CondorAdmin;		// who to send mail to in case of trouble
char		*MyName;			// name this program was invoked by
BOOLEAN		MailFlag;			// true if we should send mail about problems
BOOLEAN		VerboseFlag;		// true if we should produce verbose output
BOOLEAN		RmFlag;				// true if we should remove extraneous files
List<ProcObj>	*ProcList;		// all processes in current job queue
List<char>	*BadFiles;			// list of files which don't belong


	// prototypes of local interest
void usage();
void send_mail_file();
void init_params();
void check_spool_dir();
void check_execute_dir();
void check_log_dir();
void bad_file( char *, char * );
void good_file( char *, char * );
void produce_output();
BOOLEAN is_ckpt_file( const char *name );
BOOLEAN is_v2_ckpt( const char *name );
BOOLEAN is_v3_ckpt( const char *name );
BOOLEAN cluster_exists( int );
BOOLEAN proc_exists( int, int );
BOOLEAN do_unlink( const char *path );
BOOLEAN remove_directory( const char *name );
int do_stat( const char *path, struct stat *buf );

/*
  This primitive class is used to contain and search arrays of strings.  These
  are used for the lists of well known files which are legitimate in the
  various directories we will be checking.
*/
class StringList {
public:
	StringList( char *foo[] ) { data = foo; }
	BOOLEAN contains( const char * );
private:
	char	**data;
};

/*
  Files which are legitimate in the "spool" directory.
*/
char *ValidSpoolFiles[] = {
	"job_queue.dir",
	"job_queue.pag",
	"history",
	0
};


/*
  Files which are legitimate in the "log" directory.
*/
char *ValidLogFiles[] = {
	"MasterLog",
	"MasterLog.old",
	"NegotiatorLog",
	"NegotiatorLog.old",
	"CollectorLog",
	"CollectorLog.old",
	"SchedLog",
	"SchedLog.old",
	"ShadowLog",
	"ShadowLog.old",
	"StartLog",
	"StartLog.old",
	"StarterLog",
	"StarterLog.old",
	"KbdLog",
	"KbdLog.old",
	"GetHistoryLog",
	"GetHistoryLog.old",
	"ShadowLock",
	"priorities.pag",
	"priorities.dir",
	"machine_status",
	0
};

/*
  Tell folks how to use this program.
*/
void
usage()
{
	fprintf( stderr, "Usage: %s [-mail] [-remove] [-verbose]\n", MyName );
	exit( 1 );
}

int
main( int argc, char *argv[] )
{
	char *str;

		// Initialize things
	MyName = argv[0];
	config( MyName, (CONTEXT *)0 );
	init_params();
	BadFiles = new List<char>;

		// Parse command line arguments
	for( argv++; *argv; argv++ ) {
		if( (*argv)[0] == '-' ) {
			switch( (*argv)[1] ) {
			
			  case 'v':
				VerboseFlag = TRUE;
				break;

			  case 'm':
				MailFlag = TRUE;
				break;

			  case 'r':
				RmFlag = TRUE;
				break;

			  default:
				usage();

			}
		} else {
			usage();
		}
	}


		// Do the file checking
	check_spool_dir();
	check_execute_dir();
	check_log_dir();


		// Produce output, either on stdout or by mail
	if( !BadFiles->IsEmpty() ) {
		produce_output();
	}

		// Clean up
	for( BadFiles->Rewind(); str = BadFiles->Next(); ) {
		delete( str );
	}
	delete BadFiles;

#if defined(ALLOC_DEBUG)
	print_alloc_stats( "End of main" );
#endif

	return 0;
}

/*
  As the program runs, we create a list of messages regarding the status
  of file in the list BadFiles.  Now we use that to produce all output.
  If MailFlag is set, we send the output to the condor administrators
  via mail, otherwise we just print it on stdout.
*/
void
produce_output()
{
	char	*str;
	FILE	*mailer;
	char	cmd[ 1024 ];

	sprintf( cmd, "%s %s", BIN_MAIL, CondorAdmin );

	if( MailFlag ) {
		if( (mailer=popen(cmd,"w")) == NULL ) {
			EXCEPT( "Can't do popen(%s,\"w\")", cmd );
		}
	} else {
		mailer = stdout;
	}

	if( MailFlag ) {
		fprintf( mailer, "To: %s\n", CondorAdmin );
		fprintf( mailer, "Subject: Junk Condor Files\n" );
		fprintf( mailer, "\n" );
		fprintf( mailer,
			"These files should be removed from <%s>:\n\n", my_hostname()
		);
	}

	for( BadFiles->Rewind(); str = BadFiles->Next(); ) {
		fprintf( mailer, "%s\n", str );
	}

	if( MailFlag ) {
		pclose( mailer );
	}

}




/*
  Check the condor spool directory for extraneous files.  Files found
  there should either be on a list of well known files, or should be
  checkpoint files belonging to jobs currently in the job queue.  We
  create a global list of all jobs in the queue for use by lower level
  routines which will check actual files aginst the job queue to see
  if they are legitimate checkpoints.
*/
void
check_spool_dir()
{
	char	*f;
	Directory dir(Spool);
	StringList well_known_list(ValidSpoolFiles);
	char	queue_name[_POSIX_PATH_MAX];
	DBM	*q;


		/* Open and lock the job queue */
	(void)sprintf( queue_name, "%s/job_queue", Spool );
	if( (q=OpenJobQueue(queue_name,O_RDONLY,0)) == NULL ) {
		EXCEPT( "OpenJobQueue(%s)", queue_name );
	}
    if( LockJobQueue(q,READER) < 0 ) {
		EXCEPT( "Can't lock job queue for READING\n" );
	}

	// clear_alloc_stats();
	// print_alloc_stats( "Beginning create_list()" );

		// Create a list of all procs corresponding to the complete job queue
	ProcList = ProcObj::create_list( q, 0 );

		// Check each file in the directory
	while( f = dir.Next() ) {
			// see if it's on the list
		if( well_known_list.contains(f) ) {
			good_file( Spool, f );
			continue;
		}

			// see if it's a legitimate checkpoint
		if( is_ckpt_file(f) ) {
			good_file( Spool, f );
			continue;
		}

			// must be bad
		bad_file( Spool, f );
	}

		// Clean up
    if( LockJobQueue(q,UNLOCK) < 0 ) {
		EXCEPT( "Can't unlock job queue\n");
	}
	CloseJobQueue( q );
	ProcObj::delete_list( ProcList );

	// print_alloc_stats( "End of check_spool_dir" );
}

/*
  Given the name of a file in the spool directory, return TRUE if it's a
  legitimate checkpoint file, and FALSE otherwise.  If the name starts
  with "cluster", it should be a V3 style checkpoint.  Otherwise it is
  either a V2 style checkpoint, or not a checkpoint at all.
*/
BOOLEAN
is_ckpt_file( const char *name )
{

	// clear_alloc_stats();

	if( strstr(name,"cluster") ) {
		return is_v3_ckpt( name );
	} else {
		return is_v2_ckpt( name );
	}

	// print_alloc_stats( "End of is_ckpt_file");
}


/*
  Grab an integer value which has been embedded in a file name.  We're given
  a name to examine, and a pattern to search for.  If the pattern exists,
  then the number we want follows it immediately.  If the pattern doesn't
  exist, we return -1.
*/
inline int
grab_val( const char *str, const char *pattern )
{
	char	*ptr;

	if( ptr = strstr(str,pattern) ) {
		return atoi(ptr + strlen(pattern) );
	}
	return -1;
}

/*
  We're given the name of a file which appears to be a V2 checkpoint file.
  We try to dig out the cluster/proc ids, and search the job queue for
  a corresponding process.  We return TRUE if we find it, and FALSE
  otherwise.

  V2 checkpoint files formats are:
	  job<#>.ickpt		- initial checkpoint file
	  job<#>.ckpt.<#>	- specific checkpoint file
*/
BOOLEAN
is_v2_ckpt( const char *name )
{
	int		cluster;
	int		proc;

	cluster = grab_val( name, "job" );
	proc = grab_val( name, ".ckpt." );

	if( proc < 0 ) {
		return cluster_exists( cluster );
	} else {
		return proc_exists( cluster, proc );
	}
}

/*
  We're given the name of a file which appears to be a V3 checkpoint file.
  We try to dig out the cluster/proc ids, and search the job queue for
  a corresponding process.  We return TRUE if we find it, and FALSE
  otherwise.

  V3 checkpoint file formats are:
	  cluster<#>.ickpt.subproc<#>		- initial checkpoint file
	  cluster<#>.proc<#>.subproc<#>		- specific checkpoint file
*/
BOOLEAN
is_v3_ckpt( const char *name )
{
	int		cluster;
	int		proc;
	int		subproc;

	cluster = grab_val( name, "cluster" );
	proc = grab_val( name, ".proc" );
	subproc = grab_val( name, ".subproc" );

		
	if( proc < 0 ) {
		return cluster_exists( cluster );
	} else {
		return proc_exists( cluster, proc );
	}
}

/*
  Check to see whether a given cluster number exists in the job queue.
  Assume a higher level routine has already assembled a list of jobs in
  the list ProcList..
*/
BOOLEAN
cluster_exists( int cluster )
{
	ProcObj	*p;

	ProcList->Rewind();
	while( p = ProcList->Next() ) {
		if( p->get_cluster_id() == cluster ) {
			return TRUE;
		}
	}
	return FALSE;
}

/*
  Check to see whether a given cluster and process number exist in the
  job queue.  Assume a higher level routine has already assembled a
  list of jobs in ProcList.
*/
BOOLEAN
proc_exists( int cluster, int proc )
{
	ProcObj	*p;

	ProcList->Rewind();
	while( p = ProcList->Next() ) {
		if( p->get_cluster_id() == cluster && p->get_proc_id() == proc ) {
			return TRUE;
		}
	}
	return FALSE;
}

/*
  Scan the execute directory looking for bogus files.
*/ 
void
check_execute_dir()
{
	char	*f;
	Directory dir( Execute );
	char	pathname[_POSIX_PATH_MAX];
	int		age;
	struct stat	buf;


	while( f = dir.Next() ) {

			// if we know the state of the machine, we can use a simple
			// algorithm.  If we are hosting a job - anything goes, otherwise
			// the execute directory should be empty.
		switch( get_machine_status() ) {
			case NO_JOB:	// shouldn't get this, but means no job on board
			case M_IDLE:	// could accept condor job, but doesn't have one now
			case USER_BUSY:	// in use by owner - no condor job on board
				bad_file( Execute, f );		// directory should be empty
				continue;
			case JOB_RUNNING:		// has condor job
			case SUSPENDED:			// has job, but temporarily suspended
			case CHECKPOINTING:		// has job, in process of leaving
			case KILLED:			// has job, being forced off
				good_file( Execute, f );	// let anything go here
				continue;
			default:
				break; // not sure of the state, have to make some guesses...
		}

		// If we get here, get_machine_status didn't return something
		// we exepected.  We can't make any assumptions about the
		// state of the machine, so we fall back on some messier
		// heruistics.

			// stat the file
		if( !do_stat(pathname,&buf) ) {
			good_file( Execute, f ); // got rm'd while we were doing this - OK
			continue;
		}

			// leave subdirectories alone
		if( S_ISDIR(buf.st_mode) ) {
			good_file( Execute, f );
			continue;
		}

			// Actual files should be checkpoints.  For jobs which
			// havn't disabled checkpointing, they would be
			// rewritten at every checkpoint.  If a job has asked not
			// to be checkpointed though, it would not be re-written.
			// In that case, we guess there is a problem if the file
			// is over a day old.  

		age = time(0) - buf.st_ctime;
		if( age > MaxCkptInterval + 24 * HOUR ) {
			bad_file( Execute, f );
		} else {
			good_file( Execute, f );
		}
	}

}

/*
  Scan the log directory for bogus files.  All files in the log directory
  should have well known names.
*/
void
check_log_dir()
{
	char	*f;
	Directory dir(Log);
	StringList well_known_list(ValidLogFiles);

	while( f = dir.Next() ) {
		if( well_known_list.contains(f) ) {
			good_file( Log, f );
		} else {
			bad_file( Log, f );
		}
	}
}
	

extern "C" int
SetSyscalls( int foo ) { return foo; }

/*
  Initialize information from the configuration files.
*/
void
init_params()
{
	char	*tmp;

	Spool = param("SPOOL");
    if( Spool == NULL ) {
        EXCEPT( "SPOOL not specified in config file\n" );
    }

	Log = param("LOG");
    if( Log == NULL ) {
        EXCEPT( "LOG not specified in config file\n" );
    }

	Execute = param("EXECUTE");
    if( Execute == NULL ) {
        EXCEPT( "EXECUTE not specified in config file\n" );
    }

    if( (CondorAdmin = param("CONDOR_ADMIN")) == NULL ) {
        EXCEPT( "CONDOR_ADMIN not specified in config file" );
    }

}

/*
  Return TRUE if our string list contains the given string, and FALSE
  otherwise.
*/
StringList::contains( const char *st )
{
	char	**ptr;

	for( ptr=data; *ptr; ptr++ ) {
		if( strcmp(st,*ptr) == MATCH ) {
			return TRUE;
		}
	}
	return FALSE;
}

/*
  Unlink a file we want to get rid of.  If its a simple file, we do
  it ourself.  If its a directory, we call remove_directory() to do
  it.  The remove_directory() routine will call this routine recursively
  if the directory is not emty.
*/
BOOLEAN
do_unlink( const char *path )
{
	int		status;
	struct stat	buf;

		// stat the file
	if( !do_stat(path,&buf) ) {
		return TRUE; /* got rm'd while we were doing this */
	}

	if( S_ISDIR(buf.st_mode) ) {
		return remove_directory( path );
	}

	status = unlink( path );
	if( status < 0 && errno == EACCES ) {
		set_condor_euid();			// Try again as Condor
		status = unlink( path );
		set_root_euid();
	}

	if( status == 0 || errno == ENOENT ) {
		return TRUE;
	} else {
		return FALSE;
	}
}

/*
  We want to stat the given file. We are operating as root, but root
  == nobody across most NFS file systems, so we may have to do it as
  condor.  If we succeed, we return TRUE, and if the file has already
  been removed we return FALSE.  If we cannot do it as either root
  or condor, we bomb with an exception.
*/
int
do_stat( const char *path, struct stat *buf )
{
		// try it as root
	if( stat(path,buf) < 0 ) {
		switch( errno ) {
		  case ENOENT:	// got rm'd while we were doing this
			return FALSE;
		  case EACCES:	// permission denied, try as condor
			set_condor_euid();
			if( stat(path,buf) < 0 ) {
				EXCEPT( "stat(%s,0x%x)", path, &buf );
			}
			set_root_euid();
		}
	}
	return TRUE;
}

/*
  Remove a directory and all its children.  We do this by traversing the
  directory can calling do_unlink() on each file.  If the file turns
  out to be a subdirectory, then do_unlink() will call this routine
  recursively to remove it.  After the current directory is empty, we
  can remove it.
*/
BOOLEAN
remove_directory( const char *name )
{
	char		*f;
	Directory	dir(name);
	int			status;
	char		pathname[ _POSIX_PATH_MAX ];

		// Remove files and subdirectories
	while( f = dir.Next() ) {
		sprintf( pathname, "%s/%s", name, f );
		if( !do_unlink(pathname) ) {
			return FALSE;
		}
	}

		// Remove now empty directory
	status = rmdir( name );
	if( status < 0 && errno == EACCES) {
		set_condor_euid();			// try again as Condor
		status = rmdir(name);
		set_root_euid();
	}

	if( status == 0 || errno == ENOENT ) {
		return TRUE;
	} else {
		return FALSE;
	}
}

/*
  Produce output for a file that has been determined to be legitimate.
*/
void
good_file( char *dir, char *name )
{

	if( VerboseFlag ) {
		printf( "\"%s/%s\" - OK\n", dir, name );
	}
}

/*
  Produce output for a file that has been determined to be bogus.  We also
  remove the file here if that has been called for in our command line
  arguments.
*/
void
bad_file( char *dir, char *name )
{
	char	pathname [_POSIX_PATH_MAX];
	char	buf[_POSIX_PATH_MAX + 512];

	if( VerboseFlag ) {
		printf( "%s/%s - BAD\n", dir, name );
	}


	sprintf( pathname, "%s/%s", dir, name );

	if( RmFlag ) {
		if( do_unlink(pathname) ) {
			sprintf( buf, "\"%s\" - Removed", pathname );
		} else {
			sprintf( buf, "\"%s\" - Can't Remove", pathname );
		}
	} else {
		sprintf( buf, "\"%s\" - Not Removed", pathname );
	}
	BadFiles->Append( string_copy(buf) );
}
