/* 
** Copyright 1994 by Miron Livny, and Mike Litzkow
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

/*
  This is the interface between the condor_shadow and a logging facility
  which makes information about the history and progress of Condor jobs
  available directly to users.  The actual logging facility is implemented
  in "user_log.c".
*/

#define _POSIX_SOURCE

#if defined(Solaris) /* ..dhaval 7/18 */
#include "_condor_fix_types.h"
#endif

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_jobqueue.h"
#include "user_log.h"
#include <limits.h>
#include <string.h>

static char *_FileName_ = __FILE__;

extern char *d_format_time( double seconds );

char * find_env( const char * name, const char * env );
static char * get_env_val( const char *str );
static void put_time( USER_LOG *lp, int msg_num, int usr, int sys );

/*
  Initialize a user log given a proc structure and a hostname where the
  job will be executing on this run.  The name of the log file is gotten
  from the process's environment.  If no log is specified, then the log
  will not be opened, and this routine will return NULL.  Other routines
  which take a user log structure as an argument will do nothing when
  passed a null pointer.
*/
USER_LOG *
InitUserLog ( PROC *p, const char *host )
{
	char	buf[ _POSIX_PATH_MAX ];
	char	*path = buf;
	char	*log_name;
	USER_LOG	*answer;

	log_name = find_env( "LOG", p->env );

	if( !log_name ) {
		return NULL;
	}

	if( log_name[0] == '/' ) {
		path = log_name;
	} else {
		sprintf( path, "%s/%s", p->iwd, log_name );
	}

	answer = OpenUserLog( p->owner, path, p->id.cluster, p->id.proc, 0 );
	PutUserLog( answer, EXECUTE, host );
	return answer;
}

/*
  Find the value of an environment value, given the condor style envrionment
  string which may contain several variable definitions separated by 
  semi-colons.
*/
char *
find_env( const char * name, const char * env )
{
	const char	*ptr;

	for( ptr=env; *ptr; ptr++ ) {
		if( strncmp(name,ptr,strlen(name)) == 0 ) {
			return get_env_val( ptr );
		}
	}
	return NULL;
}

/*
  Given a pointer to the a particular environment substring string
  containing the desired value, pick out the value and return a
  pointer to it.  N.B. this is copied into a static data area to
  avoid introducing NULL's into the original environment string,
  and a pointer into the static area is returned.
*/
static char *
get_env_val( const char *str )
{
	const char	*src;
	char *dst;
	static char	answer[512];

		/* Find the '=' */
	for( src=str; *src && *src != '='; src++ )
		;
	if( *src != '=' ) {
		EXCEPT( "Invalid environment string" );
	}

		/* Skip over any white space */
	src += 1;
	while( *src && isspace(*src) ) {
		src++;
	}

		/* Now we are at beginning of result */
	dst = answer;
	while( *src && !isspace(*src) && *src != ';' ) {
		*dst++ = *src++;
	}
	*dst = '\0';

	return answer;
}

#define USR_TIME(x) x->ru_utime.tv_sec
#define SYS_TIME(x) x->ru_stime.tv_sec

/*
  Output user logging information about resource rusage for a condor process.
*/
void
LogRusage( USER_LOG *log, int which, struct rusage *loc, struct rusage *rem)
{
	if( !log ) {
		return;
	}

	BeginUserLogBlock( log );
	if( which == THIS_RUN ) {
		put_time( log, RUN_REMOTE_USAGE, USR_TIME(rem), SYS_TIME(rem) );
		put_time( log, RUN_LOCAL_USAGE, USR_TIME(loc), SYS_TIME(loc) );
	} else {
		put_time( log, TOT_REMOTE_USAGE, USR_TIME(rem), SYS_TIME(rem) );
		put_time( log, TOT_LOCAL_USAGE, USR_TIME(loc), SYS_TIME(loc) );
	}
	EndUserLogBlock( log );
}

#define SECONDS	1
#define MINUTES	(60 * SECONDS)
#define HOURS	(60 * MINUTES)
#define DAYS	(24 * HOURS)

static void
put_time( USER_LOG *lp, int msg_num, int usr_secs, int sys_secs )
{
	int usr_days, usr_hours, usr_minutes;
	int sys_days, sys_hours, sys_minutes;

	usr_days = usr_secs / DAYS;
	usr_secs %= DAYS;
	sys_days = sys_secs / DAYS;
	sys_secs %= DAYS;

	usr_hours = usr_secs / HOURS;
	usr_secs %= HOURS;
	sys_hours = sys_secs / HOURS;
	sys_secs %= HOURS;

	usr_minutes = usr_secs / MINUTES;
	usr_secs %= MINUTES;
	sys_minutes = sys_secs / MINUTES;
	sys_secs %= MINUTES;

	PutUserLog( lp, msg_num,
		usr_days, usr_hours, usr_minutes, usr_secs,
		sys_days, sys_hours, sys_minutes, sys_secs
	);
}
