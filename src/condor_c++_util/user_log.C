/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

 

#include "condor_common.h"
#include "condor_debug.h"
#include <stdarg.h>
#include "user_log.c++.h"
#include <time.h>
#include "condor_uid.h"

static char *_FileName_ = __FILE__;

static const char SynchDelimiter[] = "...\n";

static void display_ids();
extern "C" char *find_env (const char *, const char *);
extern "C" char *get_env_val (const char *);

UserLog::UserLog (const char *owner, const char *file, int c, int p, int s)
{
	UserLog();
	initialize (owner, file, c, p, s);
}

/* --- The following two functions are taken from the shadow's ulog.c --- */
/*
  Find the value of an environment value, given the condor style envrionment
  string which may contain several variable definitions separated by 
  semi-colons.
*/
char *
find_env( const char * name, const char * env )
{
    const char  *ptr;

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
char *
get_env_val( const char *str )
{
    const char  *src;
    char *dst;
    static char answer[512];

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

void UserLog::
initialize( const char *file, int c, int p, int s )
{
	int 			fd;

		// Save parameter info
	path = new char[ strlen(file) + 1 ];
	strcpy( path, file );
	in_block = FALSE;

	if (fp) fclose(fp);

	if( (fd = open( path, O_CREAT | O_WRONLY, 0664 )) < 0 ) {
		EXCEPT( "open(%s) failed with errno %d", path, errno );
	}

		// attach it to stdio stream
	if( (fp = fdopen(fd,"w")) == NULL ) {
		EXCEPT( "fdopen(%d)", fd );
	}

		// set the stdio stream for line buffering
	if( setvbuf(fp,NULL,_IOLBF,BUFSIZ) < 0 ) {
		EXCEPT( "setvbuf" );
	}

	// prepare to lock the file
	lock = new FileLock( fd );

	initialize(c, p, s);
}

void UserLog::
initialize( const char *owner, const char *file, int c, int p, int s )
{
	priv_state		priv;
	int 			fd;

		// Save parameter info
	path = new char[ strlen(file) + 1 ];
	strcpy( path, file );
	in_block = FALSE;

	init_user_ids(owner);

	priv = set_user_priv();

	if (fp) fclose(fp);

	if( (fd = open( path, O_CREAT | O_WRONLY, 0664 )) < 0 ) {
		EXCEPT( "open(%s) failed with errno %d", path, errno );
	}

		// attach it to stdio stream
	if( (fp = fdopen(fd,"w")) == NULL ) {
		EXCEPT( "fdopen(%d)", fd );
	}

		// set the stdio stream for line buffering
	if( setvbuf(fp,NULL,_IOLBF,BUFSIZ) < 0 ) {
		EXCEPT( "setvbuf" );
	}

		// prepare to lock the file
	lock = new FileLock( fd );

	initialize(c, p, s);

		// get back to whatever UID and GID we started with
	set_priv(priv);
}

void UserLog::
initialize( int c, int p, int s )
{
	cluster = c;
	proc = p;
	subproc = s;
}

UserLog::~UserLog()
{
	delete [] path;
	delete lock;
	if (fp != 0) fclose( fp );
}

void
UserLog::display()
{
	dprintf( D_ALWAYS, "Path = \"%s\"\n", path );
	dprintf( D_ALWAYS, "Job = %d.%d.%d\n", proc, cluster, subproc );
	dprintf( D_ALWAYS, "fp = 0x%x\n", fp );
	lock->display();
	dprintf( D_ALWAYS, "in_block = %s\n", in_block ? "TRUE" : "FALSE" );
}

int UserLog::
writeEvent (ULogEvent *event)
{
	// the the log is not initialized, don't bother --- just return OK
	if (path == 0) return 1;
	if (!lock) return 0;
	if (!event) return 0;

	int retval;
	priv_state priv = set_user_priv();

	// fill in event context
	event->cluster = cluster;
	event->proc = proc;
	event->subproc = subproc;

	lock->obtain (WRITE_LOCK);
	fseek (fp, 0, SEEK_END);
	retval = event->putEvent (fp);
	if (!retval) fputc ('\n', fp);
	if (fprintf (fp, SynchDelimiter) < 0) retval = 0;
	lock->release ();
	set_priv( priv );
	return retval;
}
	
void
UserLog::put( const char *fmt, ... )
{
	va_list		ap;
	va_start( ap, fmt );

	if( !fp ) {
		return;
	}

	if( !in_block ) {
		lock->obtain( WRITE_LOCK );
		fseek( fp, 0, SEEK_END );
	}

	output_header();
	vfprintf( fp, fmt, ap );

	if( !in_block ) {
		lock->release();
	}
}

void
UserLog::begin_block()
{
	struct tm	*tm;
	time_t		clock;

	if( !fp ) {
		return;
	}

	lock->obtain( WRITE_LOCK );
	fseek( fp, 0, SEEK_END );

	(void)time(  (time_t *)&clock );
	tm = localtime( (time_t *)&clock );
	fprintf( fp, "(%d.%d.%d) %d/%d %02d:%02d:%02d\n",
		cluster, proc, subproc,
		tm->tm_mon + 1, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec
	);
	in_block = TRUE;
}

void
UserLog::end_block()
{
	if( !fp ) {
		return;
	}

	in_block = FALSE;
	lock->release();
}

void
UserLog::output_header()
{
	struct tm	*tm;
	time_t		clock;

	if( !fp ) {
		return;
	}

	if( in_block ) {
		fprintf( fp, "(%d.%d.%d) ", cluster, proc, subproc );
	} else {
		(void)time(  (time_t *)&clock );
		tm = localtime( (time_t *)&clock );
		fprintf( fp, "(%d.%d.%d) %d/%d %02d:%02d:%02d ",
			cluster, proc, subproc,
			tm->tm_mon + 1, tm->tm_mday,
			tm->tm_hour, tm->tm_min, tm->tm_sec
		);
	}
}

extern "C" LP *
InitUserLog( const char *own, const char *file, int c, int p, int s )
{
	UserLog	*answer;

	answer = new UserLog( own, file, c, p, s );
	return (LP *)answer;
}

extern "C" void
CloseUserLog( LP *lp )
{
	delete lp;
}

extern "C" void
PutUserLog( LP *lp, const char *fmt, ... )
{
	va_list		ap;
	char	buf[1024];

	if( !lp ) {
		return;
	}
	va_start( ap, fmt );
	vsprintf( buf, fmt, ap );

	((UserLog *)lp) -> put( buf );
}

extern "C" void
BeginUserLogBlock( LP *lp )
{
	if( !lp ) {
		return;
	}
	((UserLog *)lp) -> begin_block();
}

extern "C" void
EndUserLogBlock( LP *lp )
{
	if( !lp ) {
		return;
	}
	((UserLog *)lp) -> end_block();
}


// ReadUserLog class


ReadUserLog::
ReadUserLog ()
{
	fp = 0;
}


ReadUserLog::
~ReadUserLog ()
{
	if (fp) fclose(fp);
}


int ReadUserLog::
initialize (const char *file)
{	
	if ((fd = open (file, O_RDONLY, 0)) == -1)
	{
		EXCEPT ("open(%s)", file);
	}

	if ((fp = fdopen (fd, "r")) == NULL)
	{
		EXCEPT ("fdopen(%d)", fd);
	}

	return 1;
}
	

ULogEventOutcome ReadUserLog::
readEvent (ULogEvent *& event)
{
	long   filepos;
	int    eventnumber;
	int    retval1, retval2;
	
	// store file position so that if we are unable to read the event, we can
	// rewind to this location
	if (!fp || ((filepos = ftell(fp)) == -1L))
		return ULOG_UNK_ERROR;

	retval1 = fscanf (fp, "%d", &eventnumber);

	// so we don't dump core if the above fscanf failed
	if (retval1 != 1) eventnumber = 1;

	// allocate event object; check if allocated successfully
	event = instantiateEvent ((ULogEventNumber) eventnumber);
	if (!event) 
	{
		return ULOG_UNK_ERROR;
	}

	// read event from file; check for result
	retval2 = event->getEvent (fp);
		
	// check if error in reading event
	if (!retval1 || !retval2)
	{	
		// try to synchronize the log
		if (synchronize())
		{
			// if synchronization was successful, reset file position and ...
			if (fseek (fp, filepos, SEEK_SET))
			{
				EXCEPT ("fseek()");
			}
			
			// ... attempt to read the event again
			clearerr (fp);
			retval1 = fscanf (fp, "%d", &eventnumber);
			retval2 = event->getEvent (fp);

			// if failed again, we have a parse error
			if (!retval1 || !retval2)
			{
				delete event;
				event = NULL;  // To prevent FMR: Free memory read
				synchronize ();
				return ULOG_RD_ERROR;
			}
			else
			{
				return ULOG_OK;
			}
		}
		else
		{
			// if we could not synchronize the log, we don't have the full	
			// event in the stream yet; restore file position and return
			if (fseek (fp, filepos, SEEK_SET))
			{
				EXCEPT ("fseek()");
			}
			clearerr (fp);
			delete event;
			event = NULL;  // To prevent FMR: Free memory read
			return ULOG_NO_EVENT;
		}
	}
	else
	{
		// got the event successfully -- synchronize the log
		if (synchronize ())
		{
			return ULOG_OK;
		}
		else
		{
			// got the event, but could not synchronize!!  treat as incomplete
			// event
			delete event;
			event = NULL;  // To prevent FMR: Free memory read
			clearerr (fp);
			return ULOG_NO_EVENT;
		}
	}

	// will not reach here
	return ULOG_UNK_ERROR;
}


int ReadUserLog::
synchronize (void)
{
	int retval = 0;
	char buffer[32];
	while (1)
	{
		if (fgets (buffer, 32, fp) == NULL) break;

		if (strcmp (buffer, SynchDelimiter) == 0)
		{
			retval = 1;
			break;
		}
	}

	return retval;
}
