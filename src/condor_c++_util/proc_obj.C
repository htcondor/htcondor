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
#include "condor_jobqueue.h"
#include <time.h>
#include <sys/wait.h>
#include <pwd.h>
#include "proc_obj.h"

static char *_FileName_ = __FILE__;     /* Used by EXCEPT (see except.h)     */


/* GET THESE OUT OF HERE -- mike */
static char *Notifications[] = {
	"Never", "Always", "Complete", "Error"
};

static char	*Status[] = {
	"Unexpanded", "Idle", "Running", "Removed", "Completed", "Submission Err"
};

static char	*Universe[] = {
	"", "STANDARD", "PIPE", "LINDA", "PVM", "VANILLA"
};

static CurIndent;				// current indent level for display functions
static int	InvokingUid;		// user id of person ivoking this program
static char	*InvokingUserName;	// user name of person ivoking this program


static char	*format_time( int secs );
static void copy_string_array( char ** &dst, char **src, int n );
static void do_indent();
static void d_msg( const char *name, char *val );
static void d_bool( const char *name, int val );
static void d_int( const char *name, int val );
static void d_enum( char *table[], const char *name, int val );
static void d_date( const char *name, int val );
static void d_string( const char *name, char *val );
static void d_rusage( const char *name, struct rusage &val );
static void d_exit_status( const char *name, int val );
static void init_user_credentials();
extern "C" BOOLEAN xdr_proc( XDR *, GENERIC_PROC * );

/*
  Create a ProcObj given an XDR stream from which to read the data.
*/
ProcObj *
ProcObj::create( XDR * xdrs )
{
	GENERIC_PROC	proc;
	const V2_PROC	*v2 = (V2_PROC *)&proc;
	const V3_PROC	*v3 = (V3_PROC *)&proc;

	xdrs->x_op = XDR_DECODE;
	memset( &proc, 0, sizeof(proc) );

	if( !xdr_proc( xdrs, &proc ) ) {
		dprintf(D_ALWAYS, "Error reading proc struct from XDR stream\n" );
		return 0;
	}

	if (v2->id.cluster == 0 && v2->id.proc == 0) {
		return 0;
	}

	switch( v2->version_num ) {
	  case 2:
		return new V2_ProcObj( v2 );
	  case 3:
		return new V3_ProcObj( v3 );
	  default:
		EXCEPT( "Unknown proc type" );
	}
		// We can never get here
	return 0;
}

/*
  Create a ProcObj given a pointer to a generic proc struct.
*/
ProcObj *
ProcObj::create( GENERIC_PROC * proc )
{
	const V2_PROC	*v2 = (V2_PROC *)proc;
	const V3_PROC	*v3 = (V3_PROC *)proc;

	if( v2->version_num == 2 ) {
		return new V2_ProcObj( v2 );
	} else if( v3->version_num == 3 ) {
		return new V3_ProcObj( v3 );
	} else {
		EXCEPT( "Unknown proc type" );
			// We can never get here
		return 0;
	}
}

/*
  Create a ProcObj given a job queue database handle and an identifier.
*/
ProcObj *
ProcObj::create( DBM * Q, int cluster, int proc )
{
	ProcObj			*answer;
	GENERIC_PROC	*gen_p;
	PROC_ID			id;

	id.cluster = cluster;
	id.proc = proc;

	gen_p = GenericFetchProc( Q, &id );
	answer = create( gen_p );
	delete( gen_p );
	return answer;
}



/*
  Delete a V3_ProcObj.
*/
V3_ProcObj::~V3_ProcObj()
{
	int		i;

	delete [] p->owner;
	delete [] p->env;
	delete [] p->rootdir;
	delete [] p->iwd;
	delete [] p->requirements;
	delete [] p->preferences;


		// per command items
	for( i=0; i<p->n_cmds; i++ ) {
		delete [] p->cmd[i];
		delete [] p->args[i];
		delete [] p->in[i];
		delete [] p->out[i];
		delete [] p->err[i];
	}
	delete [] p->cmd;
	delete [] p->args;
	delete [] p->in;
	delete [] p->out;
	delete [] p->err;
	delete [] p->remote_usage;
	delete [] p->exit_status;

		// per pipe items
	// deal with pipes later...
	// delete [] p->pipe;

	delete p;

	// printf( "Destructed V3_ProcObj\n" );
}


/*
  Copy an array of some scalar type object.  We're given a pointer by
  reference here (dst) so that we can allocate space for the array and
  make the pointer point to that space.  Once that's done, we just copy
  over the n objects in the array.

  We use this for integers, rusage structures, and pipe descriptors.
*/
template <class Type>
static void
copy_array( Type * &dst, Type *src, int n )
{
	int		i;

	dst = new Type [ n ];
	for( i=0; i<n; i++ ) {
		dst[i] = src[i];
	}
}

/*
  Copy an array of strings.  We're given a pointer by reference here
  (dst) so that we can allocate space for the array and make the
  pointer point to that space.  We can't use the above defined template
  class becuase we want to actually copy the strings, not just copy the
  array of pointers.
*/
static void
copy_string_array( char ** &dst, char **src, int n )
{
	int		i;

	dst = new char * [ n ];
	for( i=0; i<n; i++ ) {
		dst[i] = string_copy( src[i] );
	}
}

/*
  Create a V3ProcObj from a V3_PROC pointer.
*/
V3_ProcObj::V3_ProcObj( const V3_PROC *src )
{
	p = new V3_PROC;

	p->version_num = src->version_num;
	p->id = src->id;
	p->universe = src->universe;
	p->checkpoint = src->checkpoint;
	p->remote_syscalls = src->remote_syscalls;
	p->owner = string_copy( src->owner );
	p->q_date = src->q_date;
	p->completion_date = src->completion_date;
	p->status = src->status;
	p->prio = src->prio;
	p->notification = src->notification;
	p->image_size = src->image_size;
	p->env = string_copy( src->env );

		// per command items
	p->n_cmds = src->n_cmds;
	copy_string_array( p->cmd, src->cmd, p->n_cmds );
	copy_string_array( p->args, src->args, p->n_cmds );
	copy_string_array( p->in, src->in, p->n_cmds );
	copy_string_array( p->out, src->out, p->n_cmds );
	copy_string_array( p->err, src->err, p->n_cmds );
	copy_array( p->remote_usage, src->remote_usage, p->n_cmds );
	copy_array( p->exit_status, src->exit_status, p->n_cmds );

		// per pipe items
	p->n_pipes = src->n_pipes;
	// copy_array( p->pipe, src->pipe, p->n_pipes );

	/* These specify the min and max number of machines needed for
	** a PVM job
	*/
	p->min_needed = src->min_needed;
	p->max_needed = src->max_needed;

	p->rootdir = string_copy( src->rootdir );
	p->iwd = string_copy( src->iwd );
	p->requirements = string_copy( src->requirements );
	p->preferences = string_copy( src->preferences );
	p->local_usage = src->local_usage;
}

/*
  Create a V2_ProcObj from a V2_PROC pointer.
*/
V2_ProcObj::V2_ProcObj( const V2_PROC *src )
{
	p = new V2_PROC;

	p->version_num = src->version_num;
	p->id = src->id;
	p->owner = string_copy( src->owner );
	p->q_date = src->q_date;
	p->completion_date = src->completion_date;
	p->status = src->status;
	p->prio = src->prio;
	p->notification = src->notification;
	p->image_size = src->image_size;
	p->cmd = string_copy( src->cmd );
	p->args = string_copy( src->args );
	p->env = string_copy( src->env );
	p->in = string_copy( src->in );
	p->out = string_copy( src->out );
	p->err = string_copy( src->err );
	p->rootdir = string_copy( src->rootdir );
	p->iwd = string_copy( src->iwd );
	p->requirements = string_copy( src->requirements );
	p->preferences = string_copy( src->preferences );
	p->local_usage = src->local_usage;
	p->remote_usage = src->remote_usage;
	// printf( "Constructed V2_ProcObj\n" );
}

/*
  Delete a V2_ProcObj.
*/
V2_ProcObj::~V2_ProcObj( )
{
	delete [] p->owner;
	delete [] p->cmd;
	delete [] p->args;
	delete [] p->env;
	delete [] p->in;
	delete [] p->out;
	delete [] p->err;
	delete [] p->rootdir;
	delete [] p->iwd;
	delete [] p->requirements;
	delete [] p->preferences;
	delete p;
	// printf( "Destructed V2_ProcObj\n" );
}

/*
  Given a string, create and return a pointer to a copy of it.  We use
  the C++ operator new to create the new string.
*/
char *
string_copy( const char *str )
{
	char	*answer;

	answer = new char [ strlen(str) + 1 ];
	strcpy( answer, str );
	return answer;
}

/*
  Indent the number of "tabs" indicated by "CurIndent".
*/
static void
do_indent()
{
	int	i;

	printf( "" );
	for( i=0; i<CurIndent; i++ ) {
		printf( "    ");
	}
}

/*
  Display a message about some value in the proc structure.  For example
  the completion date would be replaced with the message (not completed)
  for processes which have not yet completed.
*/
static void
d_msg( const char *name, char *val )
{
	do_indent();
	printf( "%s: (%s)\n", name, val );
}

/*
  Display a boolean value.
*/
static void
d_bool( const char *name, int val )
{
	do_indent();
	printf( "%s: %s\n", name, val?"TRUE":"FALSE" );
}

/*
  Display an integer value.
*/
static void
d_int( const char *name, int val )
{
	do_indent();
	printf( "%s: %d\n", name, val );
}

/*
  Display an "enumeration" value.  These are really just #defined
  constants, and we have a table with corresponding strings.  This
  should really be replaced with a mechanism which allows the strings
  to be defined in the same include file where the constants (or
  enum) are defined.
*/
static void
d_enum( char *table[], const char *name, int val )
{
	do_indent();
	printf( "%s: %s\n", name, table[val] );
}

/*
  Display a date which is encoded in seconds since the UNIX "epoch".
*/
static void
d_date( const char *name, int val )
{
	do_indent();
	printf( "%s: %s", name, ctime(&val) );
}

/*
  Display a string value.
*/
static void
d_string( const char *name, char *val )
{
	do_indent();
	printf( "%s: \"%s\"\n", name, val );
}

/*
  Display an rusage value.
*/
static void
d_rusage( const char *name, struct rusage &val )
{
	int		usr_sec = val.ru_utime.tv_sec;
	int		sys_sec = val.ru_stime.tv_sec;
	int		tot_sec = usr_sec + sys_sec;

	do_indent();
	printf( 
		"%s %s: %s\n", name, "(user)", format_time(usr_sec)
	);

	do_indent();
	printf( 
		"%s %s: %s\n", name, "(system)", format_time(sys_sec)
	);

	do_indent();
	printf( 
		"%s %s: %s\n", name, "(total)", format_time(tot_sec)
	);
}

/*
  Display a POSIX style process exit status.
*/
static void
d_exit_status( const char *name, int val )
{
	do_indent();
	printf( "%s: ", name );
	if( WIFEXITED(val) ) {
		printf( 
			"Exited normally with status %d\n", WEXITSTATUS(val)
		);
		return;
	}

	if( WIFSIGNALED(val) ) {
		printf( 
			"Exited abnormally with signal %d\n", WTERMSIG(val)
		);
		return;
	}

	if( WIFSTOPPED(val) ) {
		printf( 
			"Stopped with signal %d\n", WSTOPSIG(val)
		);
		return;
	}
}

/*
  Display all those elements in the proc structure which exist
  on a per/process basis.
*/
void
d_per_cmd( V3_PROC *p, int i )
{
	d_string( "cmd", p->cmd[i] );
	CurIndent += 1;
	d_string( "args", p->args[i] );
	d_string( "in", p->in[i] );
	d_string( "out", p->out[i] );
	d_string( "err", p->err[i] );
	d_rusage( "remote_usage", p->remote_usage[i] );
	if( p->status == COMPLETED ) {
		d_exit_status( "exit_status", p->exit_status[i] );
	}
	CurIndent -= 1;
}

/*
  Display all those elements in a proc structure which exist on
  a per/pipe basis.
*/
void
d_per_pipe( V3_PROC *p, int i )
{
	P_DESC	*t = &p->pipe[1];
	do_indent();
	printf( "pipe: %s < %s > %s\n",
		p->cmd[t->writer], t->name, p->cmd[t->reader]
	);
}

/*
  Display those elements which are common to both the V2_PROC and V3_PROC
  structures.
*/
template <class Type>
static void
display_common_elements( Type *p )
{
	d_int( "version_num", p->version_num );
	d_int( "prio", p->prio );
	d_string( "owner", p->owner );
	d_date( "q_date", p->q_date );

	if( p->status == COMPLETED ) {
		if( p->completion_date == 0 ) {
			d_msg( "completion_date", "not recorded" );
		} else {
			d_date( "completion_date", p->completion_date );
		}
	} else {
		d_msg( "completion_date", "not completed" );
	}

	d_enum( Status, "status", p->status );
	d_int( "prio", p->prio );
	d_enum( Notifications, "notification", p->notification );

	if( p->image_size ) {
		d_int( "image_size", p->image_size );
	} else {
		d_msg( "image_size", "not recorded" );
	}

	d_string( "env", p->env );
	d_string( "rootdir", p->rootdir );
	d_string( "iwd", p->iwd );
	d_string( "requirements", p->requirements );
	d_string( "preferences", p->preferences );
	d_rusage( "local_usage", p->local_usage );
}

/*
  Display the V3_ProcObj in painstaking detail.
*/
void
V3_ProcObj::display()
{
	int		i;

	printf( "Proc %d.%d:\n", p->id.cluster, p->id.proc );
	CurIndent += 1;

		// Display stuff which is common between V2 and V3
	display_common_elements( p );

		// Display stuff which is new to V3
	d_bool( "checkpoint", p->checkpoint );
	d_bool( "remote_syscalls", p->remote_syscalls );
	d_enum( Universe, "universe", p->universe );
	d_int( "min_needed", p->min_needed );
	d_int( "max_needed", p->max_needed );


		// Display stuff which exists on a per/command basis
	for( i=0; i<p->n_cmds; i++ ) {
		d_per_cmd( p, i );
	}

		// Display stuff which exists on a per/pipe basis
	for( i=0; i<p->n_pipes; i++ ) {
		d_per_pipe( p, i );
	}

	CurIndent -= 1;
}

/*
  Display the V2_ProcObj in painstaking detail.
*/
void
V2_ProcObj::display()
{
	printf( "Proc %d.%d:\n", p->id.cluster, p->id.proc );
	CurIndent += 1;

		// Display items which are common between V2 and V3
	display_common_elements( p );

		// In V3 there is an array of commands, and these items are
		// separate for each.  Here we only have a single command.
	d_string( "cmd", p->cmd );
	d_string( "args", p->args );
	d_string( "in", p->in );
	d_string( "out", p->out );
	d_string( "err", p->err );
	d_rusage( "remote_usage", p->remote_usage );

	CurIndent -= 1;
}


/*
  Format a time value which is encoded as seconds since the UNIX
  "epoch".  We return a string in the format dd+hh:mm:ss, indicating
  days, hours, minutes, and seconds.  The string is in static data
  space, and will be overwritten by the next call to this function.
*/
static char	*
format_time( int tot_secs )
{
	int		days;
	int		hours;
	int		min;
	int		secs;
	static char	answer[25];

	days = tot_secs / DAY;
	tot_secs %= DAY;
	hours = tot_secs / HOUR;
	tot_secs %= HOUR;
	min = tot_secs / MINUTE;
	secs = tot_secs % MINUTE;

	(void)sprintf( answer, "%3d+%02d:%02d:%02d", days, hours, min, secs );
	return answer;
}

/*
  Fetch a GENERIC_PROC object from a job queue database given its id.
*/
GENERIC_PROC
*GenericFetchProc( DBM * Q, PROC_ID * id )
{
	GENERIC_PROC	*answer;
	PROC			*p;

	answer = new GENERIC_PROC;
	p = &(answer->v3_proc);
	p->id.cluster = id->cluster;
	p->id.proc = id->proc;
	if( FetchProc(Q,p) == 0 ) {
		return answer;
	} else {
		printf( "Cannot fetch proc %d.%d\n", id->cluster, id->proc );
		return NULL;
	}
}

/*
  For creating lists we will use the ScanJobQueue function.  This
  function takes a pointer to another function which will be called for
  every proc structure in the queue.  That second function will be
  passed a pointer to a proc structure as its only parameter, so we
  define a couple of global variables here to allow this function to
  build the list.
*/

#if defined(OSF1)
#pragma define_template List<ProcObj>
#pragma define_template Item<ProcObj>
#endif

	// The list we are building
static List<ProcObj>	*MyList;

	// A "filtering" function.  This function examines a proc structure to
	// determine whether it should be included in the list we are building.
static P_FILTER			MyFilt;

	// The function which actually puts a proc on a list.
void add_proc_to_list( PROC *p );

/*
  Create a list of ProcObj given an XDR stream from which to read and
  a filtering function.
*/
List<ProcObj> *
ProcObj::create_list( XDR * H, P_FILTER filter )
{
	MyList = new List<ProcObj>;
	MyFilt = filter;
	ScanHistory( H, add_proc_to_list );
	return MyList;
}

/*
  Create a list of ProcObj given a job queue database handle and a
  filtering function.
*/
List<ProcObj> *
ProcObj::create_list( DBM * Q, P_FILTER filter )
{
	MyList = new List<ProcObj>;
	MyFilt = filter;
	ScanJobQueue( Q, add_proc_to_list );
	return MyList;
}

/*
  Add a proc structure to the global ProcObj list if the filtering
  function says it should be included.
*/
void
add_proc_to_list( PROC *p )
{
	ProcObj	*p_obj;

	if( MyFilt && MyFilt(p) == FALSE ) {
		return;
	}
	p_obj = ProcObj::create( (GENERIC_PROC *)p );
	MyList->Append( p_obj );
}

/*
  Delete a ProcObj list - includes both the objects on the list and
  the list structure itself.
*/
void
ProcObj::delete_list( List<ProcObj> * list )
{
	ProcObj	*p;

	list->Rewind();
	while( p = list->Next() ) {
		delete( p );
	}
	delete list;
}

/*
  Return the status field from a V2_ProcObj.
*/
int
V2_ProcObj::get_status()
{
	return p->status;
}

/*
  Return the status field from a V3_ProcObj.
*/
int
V3_ProcObj::get_status()
{
	return p->status;
}

/*
  Return the prio field from a V2_ProcObj.
*/
int
V2_ProcObj::get_prio()
{
	return p->prio;
}

/*
  Return the prio field from a V3_ProcObj.
*/
int
V3_ProcObj::get_prio()
{
	return p->prio;
}

/*
  Return the cluster id  field from a V2_ProcObj.
*/
int
V2_ProcObj::get_cluster_id()
{
	return p->id.cluster;
}

/*
  Return the cluster id  field from a V3_ProcObj.
*/
int
V3_ProcObj::get_cluster_id()
{
	return p->id.cluster;
}

/*
  Return the proc id  field from a V2_ProcObj.
*/
int
V2_ProcObj::get_proc_id()
{
	return p->id.proc;
}

/*
  Return the proc id  field from a V3_ProcObj.
*/
int
V3_ProcObj::get_proc_id()
{
	return p->id.proc;
}

/*
  Return the owner id  field from a V2_ProcObj.
*/
char *
V2_ProcObj::get_owner()
{
	return p->owner;
}

/*
  Return the owner id  field from a V3_ProcObj.
*/
char *
V3_ProcObj::get_owner()
{
	return p->owner;
}

/*
  Given a string which is in the form of an expression, and an lvalue to
  look for, find and return the matching rvlaue.  This routine is very
  simple, and will only work for simple expressions.  Also note that
  data is returned in a static array, and will be overwritten by subsequent
  calls to this routine.
*/
char *
find_rval( const char *str, const char *lval )
{
	static char answer[512];
	char		tmp [ 512 ];
	const char	*src;
	char		*dst;
	char		*ptr;
	char		*rval;

		// Make an all lower case copy of the string in tmp
	for( src=str, dst = tmp; *src; dst++,src++) {
		if( isupper(*src) ) {
			*dst = tolower(*src);
		} else {
			*dst = *src;
		}
	}
	*dst = '\0';

		// look for the specified symbol
	if( (ptr = strstr(tmp,lval)) == NULL ) {
		return NULL;
	}

		// should be followed by the assignment operator "=="
	if( (ptr = strstr(ptr,"==")) == NULL ) {
		return NULL;
	}

		// skip over the "=="
	ptr += 2;
	if( !*ptr ) {
		return NULL;
	}

		// skip any white space
	while( *ptr && isspace(*ptr) ) {
		ptr++;
	}

		// should be at the leading " now
	if( *ptr != '"' ) {
		return NULL;
	}

		// strip off the ending " and return the value
	rval =  strtok( ptr, "\"" );
	strcpy( answer, rval );
	return answer;
}

/*
  Dig out the architecture requirement from the "requirements" line of a
  V2_ProcObj.  Note, this routine returns its answer in a static data
  area which will be overwritten by subsequent calls to this routine.
*/
char *
V2_ProcObj::get_arch()
{
	static char	answer[ 512 ];
	char	*ptr;

	if( ptr = find_rval(p->requirements,"arch") ) {
		strcpy( answer, ptr );
	} else {
		strcpy( answer, "???" );
	}
	return answer;
}

/*
  Dig out the architecture requirement from the "requirements" line of a
  V3_ProcObj.  Note, this routine returns its answer in a static data
  area which will be overwritten by subsequent calls to this routine.
*/
char *
V3_ProcObj::get_arch()
{
	static char	answer[ 512 ];
	char	*ptr;

	if( ptr = find_rval(p->requirements,"arch") ) {
		strcpy( answer, ptr );
	} else {
		strcpy( answer, "???" );
	}
	return answer;
}

/*
  Dig out the opsys requirement from the "requirements" line of a
  V2_ProcObj.  Note, this routine returns its answer in a static data
  area which will be overwritten by subsequent calls to this routine.
*/
char *
V2_ProcObj::get_opsys()
{
	static char	answer[ 512 ];
	char	*ptr;

	if( ptr = find_rval(p->requirements,"opsys") ) {
		strcpy( answer, ptr );
	} else {
		strcpy( answer, "???" );
	}
	return answer;
}

/*
  Dig out the opsys requirement from the "requirements" line of a
  V3_ProcObj.  Note, this routine returns its answer in a static data
  area which will be overwritten by subsequent calls to this routine.
*/
char *
V3_ProcObj::get_opsys()
{
	static char	answer[ 512 ];
	char	*ptr;

	if( ptr = find_rval(p->requirements,"opsys") ) {
		strcpy( answer, ptr );
	} else {
		strcpy( answer, "???" );
	}
	return answer;
}

/*
  Format a date expressed in "UNIX time" into "month/day hour:minute".
*/
static char *
format_date( time_t date )
{
	static char	buf[ 12 ];
	struct tm	*tm;

	tm = localtime( &date );
	sprintf( buf, "%2d/%-2d %02d:%02d",
		(tm->tm_mon)+1, tm->tm_mday, tm->tm_hour, tm->tm_min
	);
	return buf;
}

/*
  Encode a status from a PROC structure as a single letter suited for
  printing.
*/
static char
encode_status( int status )
{
	switch( status ) {
	  case UNEXPANDED:
		return 'U';
	  case IDLE:
		return 'I';
	  case RUNNING:
		return 'R';
	  case COMPLETED:
		return 'C';
	  case REMOVED:
		return 'X';
	  default:
		return ' ';
	}
}

/*
  Put as much of the two strings as will fit in the given number of spaces
  into the given buffer and return it.
*/
static char *
shorten( char *buf, const char *str1, const char *str2, int len )
{
	const char	*src;
	char *dst;

	src = str1;
	dst = buf;
	while( len && *src ) {
		*dst++ = *src++;
		len -= 1;
	}
	if( len > 0 ) {
		*dst++ = ' ';
		len -= 1;
	}
	src = str2;
	while( len && *src ) {
		*dst++ = *src++;
		len -= 1;
	}
	*dst = '\0';
	return buf;
}

/*
  Print a header for a "short" display of a proc structure.  The short
  display is the one used by condor_q.  N.B. the columns defined in
  this routine must match those used in the short_print() routine
  defined below.
*/
void
ProcObj::short_header()
{
	printf( " %-7s %-14s %11s %12s %-2s %-3s %-4s %-18s\n",
		"ID",
		"OWNER",
		"SUBMITTED",
		"CPU_USAGE",
		"ST",
		"PRI",
		"SIZE",
		"CMD"
	);
}

/*
  Print a line of data for the "short" display of a PROC structure.  The
  "short" display is the one used by "condor_q".  N.B. the columns used
  by this routine must match those defined by the short_header routine
  defined above.
*/
inline void
short_print(
	int cluster,
	int proc,
	const char *owner,
	int date,
	int time,
	int status,
	int prio,
	int image_size,
	const char *cmd
	) {
	printf( "%4d.%-3d %-14s %-11s %-12s %-2c %-3d %-4.1f %-18s\n",
		cluster,
		proc,
		owner,
		format_date(date),
		format_time(time),
		encode_status(status),
		prio,
		image_size/1024.0,
		cmd
	);
}

/*
  Print the header used for the "special" display of a PROC structure.  The
  special display is the one used buy "condor_globalq".  N.B. the columns
  defined by this routine must match the ones used by the special_print()
  routine defined below.
*/
void
ProcObj::special_header()
{
	printf( " %-7s %-8s %11s %12s %-2s %-3s %-4s %-6s %-10s %-7s\n",
		"ID",
		"OWNER",
		"SUBMITTED",
		"CPU_USAGE",
		"ST",
		"PRI",
		"SIZE",
		"ARCH",
		"OPSYS",
		"CMD"
	);
}

/*
  Print a line of  data used for the "special" display of a PROC
  structure.  The special display is the one used by
  "condor_globalq".  N.B. the columns used by this routine must match
  the ones defined by the special_header() routine defined above.
*/
inline void
special_print(
	int cluster,
	int proc,
	const char *owner,
	int date,
	int time,
	int status,
	int prio,
	int image_size,
	const char *arch,
	const char *opsys,
	const char *cmd
	) {
	printf( "%4d.%-3d %-8s %-11s %-12s %-2c %-3d %-4.1f %-6s %-10s %-7s\n",
		cluster,
		proc,
		owner,
		format_date(date),
		format_time(time),
		encode_status(status),
		prio,
		image_size/1024.0,
		arch,
		opsys,
		cmd
	);
}

/*
  Given a V2_ProcObj, marshall the arguments and call special_print().
*/
void
V2_ProcObj::display_special()
{
	char		owner[9];
	char		arch[7];
	char		opsys[11];
	char		cmd[8];

	shorten( owner, p->owner, "", 8 );
	shorten( arch, get_arch(), "", 6 );
	shorten( opsys, get_opsys(), "", 10 );
	shorten( cmd, p->cmd, p->args, 7 );

	special_print(
		p->id.cluster,
		p->id.proc,
		owner,
		p->q_date,
		p->remote_usage.ru_utime.tv_sec,
		p->status,
		p->prio,
		p->image_size,
		arch,
		opsys,
		cmd
	);
			
}

/*
  Given a V3_ProcObj, marshall the arguments and call special_print().
*/
void
V3_ProcObj::display_special()
{
	char		owner[9];
	char		arch[7];
	char		opsys[11];
	char		cmd[8];
	int		remote_cpu = 0;
	int		i;

	shorten( owner, p->owner, "", 8 );
	shorten( arch, get_arch(), "", 6 );
	shorten( opsys, get_opsys(), "", 10 );
	shorten( cmd, p->cmd[0], p->args[0], 7 );

	for( i=0; i<p->n_cmds; i++ ) {
		remote_cpu += p->remote_usage[i].ru_utime.tv_sec;
	}

	special_print(
		p->id.cluster,
		p->id.proc,
		owner,
		p->q_date,
		remote_cpu,
		p->status,
		p->prio,
		p->image_size,
		arch,
		opsys,
		cmd
	);
			
}

/*
  Given a V2_ProcObj, marshall the arguments and call short_print().
*/
void
V2_ProcObj::display_short()
{
	char		cmd[19];
	char		owner[15];

	shorten( owner, p->owner, "", 14 );
	shorten( cmd, p->cmd, p->args, 18 );

	short_print(
		p->id.cluster,
		p->id.proc,
		owner,
		p->q_date,
		p->remote_usage.ru_utime.tv_sec,
		p->status,
		p->prio,
		p->image_size,
		cmd
	);
			
}

/*
  Given a V3_ProcObj, marshall the arguments and call short_print().
*/
void
V3_ProcObj::display_short()
{
	char	cmd[19];
	char	owner[15];
	int		remote_cpu = 0;
	int		i;

	shorten( owner, p->owner, "", 14 );
	shorten( cmd, p->cmd[0], p->args[0], 18 );

	for( i=0; i<p->n_cmds; i++ ) {
		remote_cpu += p->remote_usage[i].ru_utime.tv_sec;
	}

	short_print(
		p->id.cluster,
		p->id.proc,
		owner,
		p->q_date,
		remote_cpu,
		p->status,
		p->prio,
		p->image_size,
		cmd
	);
			
}

/*
  Alter the priority field in a V2_ProcObj.  Note that this only changes
  the in-core value.  The object must later be "stored" for this to
  affect the read job queue.
*/
void
V2_ProcObj::set_prio( int new_prio )
{
	p->prio = new_prio;
}

/*
  Alter the priority field in a V3_ProcObj.  Note that this only changes
  the in-core value.  The object must later be "stored" for this to
  affect the read job queue.
*/
void
V3_ProcObj::set_prio( int new_prio )
{
	p->prio = new_prio;
}

/*
  Store a V2_ProcObj exisiting in-core into the job queue database.
*/
void
V2_ProcObj::store( DBM *Q )
{
	StoreProc( Q, (PROC *)p );
}

/*
  Store a V3_ProcObj exisiting in-core into the job queue database.
*/
void
V3_ProcObj::store( DBM *Q )
{
	StoreProc( Q, p );
}

/*
  Set up uid and user name of the user who invoked this program in global
  data areas for later user by other routines.
*/
static void
init_user_credentials()
{
	struct passwd *pwd;
	int		uid;

	InvokingUid = getuid();

	if( (pwd=getpwuid(InvokingUid)) == NULL ) {
		EXCEPT( "Can't find password entry for user %d\n", InvokingUid );
	}

	InvokingUserName = string_copy( pwd->pw_name );
}

/*
  Given a ProcObj, return TRUE if the user who invoked this program has
  permission to modify it, and FALSE otherwise.
*/
BOOLEAN
ProcObj::perm_to_modify()
{
	if( !InvokingUserName ) {
		init_user_credentials();
	}

	if( InvokingUid == 0 )
		return TRUE;

	if( strcmp(InvokingUserName,get_owner()) == MATCH ) {
		return TRUE;
	}

	return FALSE;
}

inline float
float_time( struct timeval& tv )
{
	return (float)(tv.tv_sec) + (float)(tv.tv_usec) / 1000000.0;
}

float
V2_ProcObj::get_local_cpu()
{
	return float_time( p->local_usage.ru_utime );
}

float
V3_ProcObj::get_local_cpu()
{
	return float_time( p->local_usage.ru_utime );
}

float
V2_ProcObj::get_remote_cpu()
{
	return float_time( p->remote_usage.ru_utime );
}

float
V3_ProcObj::get_remote_cpu()
{
	float	local_cpu = 0.0;
	int		i;

	for( i=0; i<p->n_cmds; i++ ) {
		local_cpu += float_time( p->remote_usage[i].ru_utime );
	}
	return local_cpu;
}
