/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

 

#include "condor_common.h"
#include "condor_debug.h"
#include <stdarg.h>
#include "user_log.c++.h"
#include <time.h>
#include "condor_uid.h"
#include "condor_xml_classads.h"
#include "condor_config.h"
#include "utc_time.h"
#include "stat_wrapper.h"
#include "../condor_privsep/condor_privsep.h"


static const char SynchDelimiter[] = "...\n";

extern "C" char *find_env (const char *, const char *);
extern "C" char *get_env_val (const char *);
extern "C" int rotate_file(const char *old_filename, const char *new_filename);

UserLog::UserLog( void )
{
	Reset( );
}

UserLog::UserLog (const char *owner,
                  const char *domain,
                  const char *file,
				  int c,
                  int p,
                  int s,
                  bool xml, const char *gjid)
{
	Reset();
	m_use_xml = xml;

	initialize (owner, domain, file, c, p, s, gjid);
}

/* This constructor is just like the constructor above, except
 * that it doesn't take a domain, and it passes NULL for the domain and
 * the globaljobid. Hopefully it's not called anywhere by the condor code...
 * It's a convenience function, requested by our friends in LCG. */
UserLog::UserLog (const char *owner,
                  const char *file,
                  int c,
                  int p,
                  int s,
                  bool xml)
{
	Reset( );
	m_use_xml = xml;

	initialize (owner, NULL, file, c, p, s, NULL);
}

void
UserLog::Reset( void )
{
	cluster = -1;
	proc = -1;
	subproc = -1;
	in_block = FALSE; 

	m_write_user_log = true;
	m_path = NULL;
	m_fp = NULL; 
	m_lock = NULL;

	m_write_global_log = true;
	m_global_path = NULL;
	m_global_fp = NULL; 
	m_global_lock = NULL;

	m_use_xml = XML_USERLOG_DEFAULT;
	m_gjid = NULL;

#if !defined(WIN32)
	m_privsep_uid = 0;
	m_privsep_gid = 0;
#endif

	MyString	base;

	base = "";
	base += getuid();
	base += '.';
	base += getpid();
	base += '.';

	UtcTime	utc;
	utc.getTime();
	base += utc.seconds();
	base += '.';
	base += utc.microseconds();
	base += '.';

	m_global_uniq_base = strdup( base.GetCStr( ) );
	m_global_sequence = 1;
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
        return NULL;
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

bool UserLog::
open_file(const char *file, 
		  bool log_as_user, // if false, we are logging to the global file
		  FileLock* & lock, 
		  FILE* & fp )
{
	int 			fd;

	if ( file && strcmp(file,UNIX_NULL_FILE)==0 ) {
		// special case - deal with /dev/null.  we don't really want
		// to open /dev/null, but we don't want to fail in this case either
		// because this is common when the user does not want a log, but
		// the condor admin desires a global event log.
		// Note: we always check UNIX_NULL_FILE, since we canonicalize
		// to this even on Win32.
		fp = NULL;
		lock = NULL;
		return true;
	}
	
# if !defined(WIN32)
	// Unix
	int	flags = O_WRONLY | O_CREAT;
	mode_t mode = 0664;

	if (privsep_enabled() && log_as_user) {
		ASSERT(m_privsep_uid != 0);
		ASSERT(m_privsep_gid != 0);
		fd = privsep_open(m_privsep_uid, m_privsep_gid, file, flags, mode);
		if (fd == -1) {
			dprintf(D_ALWAYS,
		            "UserLog::initialize: privsep_open(\"%s\") failed\n",
			        file);
			return false;
		}
	}
	else {
		fd = safe_open_wrapper( file, flags, mode );
		if( fd < 0 ) {
			dprintf( D_ALWAYS,
			         "UserLog::initialize: "
			             "safe_open_wrapper(\"%s\") failed - errno %d (%s)\n",
			         file,
			         errno,
			         strerror(errno) );
			return false;
		}
	}

		// attach it to stdio stream
	if( (fp = fdopen(fd,"a")) == NULL ) {
		dprintf( D_ALWAYS, "UserLog::initialize: "
				 "fdopen(%i) failed - errno %d (%s)\n", fd, errno,
				 strerror(errno) );
		close( fd );
		return false;
	}
# else
	// Windows (Visual C++)
	if( (fp = safe_fopen_wrapper(file,"a+tc")) == NULL ) {
		dprintf( D_ALWAYS, "UserLog::initialize: "
				 "safe_fopen_wrapper(\"%s\") failed - errno %d (%s)\n", 
				 file, errno, strerror(errno) );
		return false;
	}

	fd = _fileno(fp);
# endif

		// set the stdio stream for line buffering
	if( setvbuf( fp,NULL,_IOLBF,BUFSIZ) < 0 ) {
		dprintf( D_ALWAYS, "setvbuf failed in UserLog::initialize\n" );
	}

	// prepare to lock the file.	
	if ( param_boolean("ENABLE_USERLOG_LOCKING",true) ) {
		lock = new FileLock( fd, fp, file );
	} else {
		lock = new FileLock( -1 );
	}

	return true;
}

bool UserLog::
initialize_global_log()
{
	bool ret_val = true;

	if (m_global_path) {
		free(m_global_path);
		m_global_path = NULL;
	}
	if (m_global_lock) {
		delete m_global_lock;
		m_global_lock = NULL;
	}
	if (m_global_fp != NULL) {
		fclose(m_global_fp);
		m_global_fp = NULL;
	}

	m_global_path = param("EVENT_LOG");
	m_global_use_xml = param_boolean("EVENT_LOG_USE_XML",false);

	if ( m_global_path ) {
		ret_val = open_file(m_global_path, false, m_global_lock, m_global_fp);

		if( m_global_path ) {
			StatWrapper		statinfo;
			if (  ( !(statinfo.Stat(m_global_path))  ) && 
				  ( !(statinfo.GetStatBuf()->st_size) )  ) {
				GenericEvent	event;
				MyString file_id;
				GenerateGlobalId( file_id );
				snprintf(event.info, sizeof(event.info),
						 "Global JobLog: "
						 "ctime=%d id=%s sequence=%d size=%ld events=%ld",
						 (int) time(NULL),
						 file_id.GetCStr(), m_global_sequence,
						 0L, 0L
						 );
				m_global_sequence++;
				dprintf( D_FULLDEBUG, "Writing log header: '%s'\n",
						 event.info );
				int		len = strlen( event.info );
				while( len < 256 ) {
					strcat( event.info, " " );
					len++;
				}
				ret_val = doWriteEvent( m_global_fp, &event,
										m_global_use_xml );
			}
		}
	}

	return ret_val;
}

bool UserLog::
initialize( const char *file, int c, int p, int s, const char *gjid)
{
		// Save parameter info
	m_path = new char[ strlen(file) + 1 ];
	strcpy( m_path, file );
	in_block = FALSE;

	if( m_fp ) {
		if( fclose( m_fp ) != 0 ) {
			dprintf( D_ALWAYS, "UserLog::initialize: "
					 "fclose(\"%s\") failed - errno %d (%s)\n", m_path,
					 errno, strerror(errno) );
		}
		m_fp = NULL;
	}

	if ( m_write_user_log && !open_file(file,true,m_lock,m_fp) ) {
		return false;
	}

	return initialize(c, p, s, gjid);
}

bool UserLog::
initialize( const char *owner, const char *domain, const char *file,
	   	int c, int p, int s, const char *gjid )
{
	priv_state		priv;

	uninit_user_ids();
	if (!  init_user_ids(owner, domain) ) {
		dprintf(D_ALWAYS, "init_user_ids() failed!\n");
		return FALSE;
	}

		// switch to user priv, saving the current user
	priv = set_user_priv();

		// initialize log file
	bool res = initialize(file,c,p,s,gjid);

		// get back to whatever UID and GID we started with
	set_priv(priv);

	return res;
}

#if !defined(WIN32)
bool
UserLog::initialize( uid_t uid,
                     gid_t gid,
                     const char *file,
                     int c,
                     int p,
                     int s,
					 const char *gjid )
{
	// this is only for use in the PrivSep world; make sure this is the case
	//
	ASSERT(privsep_enabled());

	m_privsep_uid = uid;
	m_privsep_gid = gid;

	return initialize(file, c, p, s, gjid);
}
#endif

bool UserLog::
initialize( int c, int p, int s, const char *gjid )
{
	cluster = c;
	proc = p;
	subproc = s;

		// Important for performance : note we do not re-open the global log
		// if we already have done so (i.e. if m_global_fp is not NULL).
	if ( m_write_global_log && !m_global_fp ) {
		initialize_global_log();
	}

	if(gjid) {
		m_gjid = strdup(gjid);
	}

	return TRUE;
}

UserLog::~UserLog()
{
	if (m_path) delete [] m_path;
	if (m_lock) delete m_lock;
	if (m_gjid) free(m_gjid);
	if (m_fp != NULL) fclose( m_fp );

	if (m_global_path) free(m_global_path);
	if (m_global_lock) delete m_global_lock;
	if (m_global_fp != NULL) fclose(m_global_fp);
	if (m_global_uniq_base != NULL) free( m_global_uniq_base );
}

#if 0 /* deprecated cruft */
void
UserLog::display()
{
	dprintf( D_ALWAYS, "Path = \"%s\"\n", m_path );
	dprintf( D_ALWAYS, "Job = %d.%d.%d\n", proc, cluster, subproc );
	dprintf( D_ALWAYS, "fp = %p\n", m_fp );
	lock->display();
	dprintf( D_ALWAYS, "in_block = %s\n", in_block ? "TRUE" : "FALSE" );
}
#endif

	// This method is called from doWriteEvent() - we expect the file to
	// be locked, seeked to the end of the file, and in condor priv state.
	// return true if log was rotated, either by us or someone else.
bool UserLog::
handleGlobalLogRotation()
{
	bool rotated = false;
	static long previous_filesize = 0L;
	long current_filesize = 0L;

	if (!m_global_fp) return false;
	if (!m_global_path) return false;

	StatWrapper	swrap( m_global_path );
	current_filesize = swrap.GetStatBuf()->st_size;

	int global_max_filesize = param_integer("MAX_EVENT_LOG",1000000);
	if ( current_filesize > global_max_filesize ) {
		MyString old_name(m_global_path);
		old_name += ".old";
#ifdef WIN32
		if ( m_global_fp) {
			fclose(m_global_fp);	// on win32, cannot rename an open file
			m_global_fp = NULL;
		}
#endif
		if ( rotate_file(m_global_path,old_name.Value()) == 0 ) {
			rotated = true;
			dprintf(D_ALWAYS,"Rotated event log %s at size %ld bytes\n",
					m_global_path, current_filesize);
		}
	}

	if  ( rotated == true ||						// we rotated the log
		  current_filesize < previous_filesize )	// someone else rotated it
	{
			// log was rotated, so we need to reopen/create it and also
			// recreate our lock.
		initialize_global_log();	// this will re-open and re-create locks
		rotated = true;
		if ( m_global_lock ) {
			m_global_lock->obtain(WRITE_LOCK);
			fseek (m_global_fp, 0, SEEK_END);
			current_filesize = ftell(m_global_fp);
		}
	}

	previous_filesize = current_filesize;

	return rotated;
}

int UserLog::
doWriteEvent( ULogEvent *event, bool is_global_event, ClassAd *)
{
	int success;
	FILE* fp;
	FileLock* lock;
	bool use_xml;
	priv_state priv;

	if (is_global_event) {
		fp = m_global_fp;
		lock = m_global_lock;
		use_xml = m_global_use_xml;
		priv = set_condor_priv();
	} else {
		fp = m_fp;
		lock = m_lock;
		use_xml = m_use_xml;
		priv = set_user_priv();
	}

	lock->obtain (WRITE_LOCK);
	fseek (fp, 0, SEEK_END);

		// rotate the global event log if it is too big
	if ( is_global_event ) {
		if ( handleGlobalLogRotation() ) {
				// if we rotated the log, we have a new fp and lock
			fp = m_global_fp;
			lock = m_global_lock;
		}
	}

	success = doWriteEvent( fp, event, use_xml );

	if ( fflush(fp) != 0 ) {
		dprintf( D_ALWAYS, "fflush() failed in UserLog::doWriteEvent - "
				"errno %d (%s)\n", errno, strerror(errno) );
		// Note:  should we set success to false here?
	}

	// Now that we have flushed the stdio stream, sync to disk
	// *before* we release our write lock!
	// For now, for performance, do not sync the global event log.
	if ( is_global_event == false ) {
		if ( fsync( fileno( fp ) ) != 0 ) {
			dprintf( D_ALWAYS, "fsync() failed in UserLog::writeEvent - "
					"errno %d (%s)\n", errno, strerror(errno) );
			// Note:  should we set success to false here?
		}
	}
	lock->release ();
	set_priv( priv );
	return success;
}

int
UserLog::doWriteEvent( FILE *fp, ULogEvent *event, bool use_xml )
{
	ClassAd* eventAd = NULL;
	int success = TRUE;

	if( use_xml ) {
		dprintf( D_ALWAYS, "Asked to write event of number %d.\n",
				 event->eventNumber);

		eventAd = event->toClassAd();	// must delete eventAd eventually
		MyString adXML;
		if (!eventAd) {
			success = FALSE;
		} else {
			ClassAdXMLUnparser xmlunp;
			xmlunp.SetUseCompactSpacing(FALSE);
			xmlunp.SetOutputTargetType(FALSE);
			xmlunp.Unparse(eventAd, adXML);
			if (fprintf ( fp, adXML.GetCStr()) < 0) {
				success = FALSE;
			} else {
				success = TRUE;
			}
		}
	} else {
		success = event->putEvent ( fp);
		if (!success) {
			fputc ('\n', fp);
		}
		if (fprintf ( fp, SynchDelimiter) < 0) {
			success = FALSE;
		}
	}

	if ( eventAd ) {
		delete eventAd;
	}

	return success;
}



// Return FALSE(0) on error, TRUE(1) on goodness
int UserLog::
writeEvent ( ULogEvent *event, ClassAd *param_jobad)
{
	// the the log is not initialized, don't bother --- just return OK
	if (!m_fp && !m_global_fp) {
		return TRUE;
	}
	
	// make certain some parameters we will need are initialized
	if (!event) {
		return FALSE;
	}
	if (m_fp) {
		if (!m_lock) return FALSE;
	}
	if (m_global_fp) {
		if (!m_global_lock) return FALSE;
	}

	// fill in event context
	event->cluster = cluster;
	event->proc = proc;
	event->subproc = subproc;
	event->setGlobalJobId(m_gjid);
	
	// write global event
	if ( m_write_global_log && m_global_fp && 
		 doWriteEvent(event,true,param_jobad)==FALSE ) 
	{
		return FALSE;
	}

	char *attrsToWrite = param("EVENT_LOG_JOB_AD_INFORMATION_ATTRS");
	if ( m_write_global_log && m_global_fp && attrsToWrite ) {
		ExprTree *tree;
		EvalResult result;
		char *curr;

		ClassAd *eventAd = event->toClassAd();

		StringList attrs(attrsToWrite);
		attrs.rewind();
		while ( eventAd && param_jobad && (curr=attrs.next()) ) 
		{
			if ( (tree=param_jobad->Lookup(curr)) ) {
				// found the attribute.  now evaluate it before
				// we put it into the eventAd.
				if ( tree->RArg()->EvalTree(param_jobad,&result) ) {
					// now inserted evaluated expr
					switch (result.type) {
					case LX_BOOL:
					case LX_INTEGER:
						eventAd->Assign(((Variable*)tree->LArg())->Name(),result.i);
						break;
					case LX_FLOAT:
						eventAd->Assign(((Variable*)tree->LArg())->Name(),result.f);
						break;
					case LX_STRING:
						eventAd->Assign(((Variable*)tree->LArg())->Name(),result.s);
						break;
					default:
						break;
					}
				}
			}
		}
		
		// The EventTypeNumber will get overwritten to be a JobAdInformationEvent,
		// so preserve the event that triggered us to write out the info in 
		// another attribute name called TriggerEventTypeNumber.
		if ( eventAd  ) {			
			eventAd->Assign("TriggerEventTypeNumber",event->eventNumber);
			eventAd->Assign("TriggerEventTypeName",event->eventName());
			// Now that the eventAd has everything we want, write it. 
			JobAdInformationEvent info_event;
			eventAd->Assign("EventTypeNumber",info_event.eventNumber);
			info_event.initFromClassAd(eventAd);
			info_event.cluster = cluster;
			info_event.proc = proc;
			info_event.subproc = subproc;
			doWriteEvent(&info_event,true,param_jobad);
			delete eventAd;
		}
	}

	if ( attrsToWrite ) {
		free(attrsToWrite);
	}
		
	// write ulog event
	if ( m_write_user_log && m_fp && doWriteEvent(event,false,param_jobad)==FALSE ) {
		return FALSE;
	}

	return TRUE;
}

#if 0 /* deprecated cruft */	
void
UserLog::put( const char *fmt, ... )
{
	va_list		ap;
	va_start( ap, fmt );

	if( !m_fp ) {
		return;
	}

	if( !in_block ) {
		lock->obtain( WRITE_LOCK );
		fseek( m_fp, 0, SEEK_END );
	}

	output_header();
	vfprintf( m_fp, fmt, ap );

	if( !in_block ) {
		lock->release();
	}
}

void
UserLog::begin_block()
{
	struct tm	*tm;
	time_t		clock;

	if( !m_fp ) {
		return;
	}

	lock->obtain( WRITE_LOCK );
	fseek( m_fp, 0, SEEK_END );

	(void)time(  (time_t *)&clock );
	tm = localtime( (time_t *)&clock );
	fprintf( m_fp, "(%d.%d.%d) %d/%d %02d:%02d:%02d\n",
		cluster, proc, subproc,
		tm->tm_mon + 1, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec
	);
	in_block = TRUE;
}

void
UserLog::end_block()
{
	if( !m_fp ) {
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

	if( !m_fp ) {
		return;
	}

	if( in_block ) {
		fprintf( m_fp, "(%d.%d.%d) ", cluster, proc, subproc );
	} else {
		(void)time(  (time_t *)&clock );
		tm = localtime( (time_t *)&clock );
		fprintf( m_fp, "(%d.%d.%d) %d/%d %02d:%02d:%02d ",
			cluster, proc, subproc,
			tm->tm_mon + 1, tm->tm_mday,
			tm->tm_hour, tm->tm_min, tm->tm_sec
		);
	}
}

extern "C" LP *
InitUserLog( const char *own, const char *domain, const char *file,
	   	int c, int p, int s )
{
	UserLog	*answer;

	answer = new UserLog( own, domain, file, c, p, s, NULL );
	return (LP *)answer;
}

extern "C" void
CloseUserLog( LP *lp )
{
	delete (UserLog*)lp;
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

#endif  /* deprecated cruft */

// Generates a uniq global file ID
void
UserLog::GenerateGlobalId( MyString &id )
{
	UtcTime	utc;
	utc.getTime();

	id =  m_global_uniq_base;
	id += m_global_sequence;
	id += '.';
	id += utc.seconds();
	id += '.';
	id += utc.microseconds();
}
