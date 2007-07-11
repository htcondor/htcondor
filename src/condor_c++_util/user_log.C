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
#include "../condor_privsep/condor_privsep.h"


static const char SynchDelimiter[] = "...\n";

extern "C" char *find_env (const char *, const char *);
extern "C" char *get_env_val (const char *);
extern "C" int rotate_file(const char *old_filename, const char *new_filename);

UserLog::UserLog (const char *owner,
                  const char *domain,
                  const char *file,
				  int c,
                  int p,
                  int s,
                  bool xml, const char *gjid) :
	in_block(FALSE),
	path(0),
	fp(0),
	lock(NULL)
{
#if !defined(WIN32)
	m_privsep_uid = 0;
	m_privsep_gid = 0;
#endif
	UserLog();
	use_xml = xml;

	global_path = NULL;
	global_fp = NULL;
	global_lock = NULL;

	write_user_log = true;
	write_global_log = true;

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
                  bool xml) :
	in_block(FALSE),
	path(0),
	fp(0),
	lock(NULL)
{
#if !defined(WIN32)
	m_privsep_uid = 0;
	m_privsep_gid = 0;
#endif
	UserLog();
	use_xml = xml;

	global_path = NULL;
	global_fp = NULL;
	global_lock = NULL;

	initialize (owner, NULL, file, c, p, s, NULL);
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
	
#ifndef WIN32
	// Unix
	if (privsep_enabled() && log_as_user) {
		ASSERT(m_privsep_uid != 0);
		ASSERT(m_privsep_gid != 0);
		fd = privsep_open(m_privsep_uid,
		                  m_privsep_gid,
		                  file,
		                  O_WRONLY | O_CREAT,
		                  0664);
		if (fd == -1) {
			dprintf(D_ALWAYS,
		            "UserLog::initialize: privsep_open(\"%s\") failed\n",
			        file);
			return false;
		}
	}
	else {
		fd = safe_open_wrapper( file, O_CREAT | O_WRONLY, 0664 );
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
#else
	// Windows (Visual C++)
	if( (fp = safe_fopen_wrapper(file,"a+tc")) == NULL ) {
		dprintf( D_ALWAYS, "UserLog::initialize: "
				 "safe_fopen_wrapper(\"%s\") failed - errno %d (%s)\n", 
				 file, errno, strerror(errno) );
		return false;
	}

	fd = _fileno(fp);
#endif

		// set the stdio stream for line buffering
	if( setvbuf(fp,NULL,_IOLBF,BUFSIZ) < 0 ) {
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

	if (global_path) {
		free(global_path);
		global_path = NULL;
	}
	if (global_lock) {
		delete global_lock;
		global_lock = NULL;
	}
	if (global_fp != NULL) {
		fclose(global_fp);
		global_fp = NULL;
	}

	global_path = param("EVENT_LOG");

	if ( global_path ) {
		 ret_val = open_file(global_path,false,global_lock,global_fp);
	}

	return ret_val;
}

bool UserLog::
initialize( const char *file, int c, int p, int s, const char *gjid)
{
		// Save parameter info
	path = new char[ strlen(file) + 1 ];
	strcpy( path, file );
	in_block = FALSE;

	if( fp ) {
		if( fclose( fp ) != 0 ) {
			dprintf( D_ALWAYS, "UserLog::initialize: "
					 "fclose(\"%s\") failed - errno %d (%s)\n", path,
					 errno, strerror(errno) );
		}
		fp = NULL;
	}

	if ( write_user_log && !open_file(file,true,lock,fp) ) {
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
		// if we already have done so (i.e. if global_fp is not NULL).
	if ( write_global_log && !global_fp ) {
		initialize_global_log();
	}

	if(gjid) {
		m_gjid = strdup(gjid);
	}

	return TRUE;
}

UserLog::~UserLog()
{
	if (path) delete [] path;
	if (lock) delete lock;
	if(m_gjid) free(m_gjid);
	if (fp != NULL) fclose( fp );

	if (global_path) free(global_path);
	if (global_lock) delete global_lock;
	if (global_fp != NULL) fclose(global_fp);
}

#if 0 /* deprecated cruft */
void
UserLog::display()
{
	dprintf( D_ALWAYS, "Path = \"%s\"\n", path );
	dprintf( D_ALWAYS, "Job = %d.%d.%d\n", proc, cluster, subproc );
	dprintf( D_ALWAYS, "fp = %p\n", fp );
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

	if (!global_fp) return false;

	current_filesize = ftell(global_fp);

	int global_max_filesize = param_integer("MAX_EVENT_LOG",1000000);
	if ( global_path && current_filesize > global_max_filesize ) {
		MyString old_name(global_path);
		old_name += ".old";
		if ( global_fp) {
			fclose(global_fp);	// on win32, cannot rename an open file
			global_fp = NULL;
		}
		if ( rotate_file(global_path,old_name.Value()) == 0 ) {
			rotated = true;
			dprintf(D_ALWAYS,"Rotated event log %s at size %d bytes\n",
					global_path, current_filesize);
		}
	}

	if  ( rotated == true ||						// we rotated the log
		  current_filesize < previous_filesize )	// someone else rotated it
	{
			// log was rotated, so we need to reopen/create it and also
			// recreate our lock.
		initialize_global_log();	// this will re-open and re-create locks
		rotated = true;
		if ( global_lock ) {
			global_lock->obtain(WRITE_LOCK);
			fseek (global_fp, 0, SEEK_END);
			current_filesize = ftell(global_fp);
		}
	}

	previous_filesize = current_filesize;

	return rotated;
}

int UserLog::
doWriteEvent(ULogEvent *event, bool is_global_event, ClassAd *)
{
	int success;
	FILE* local_fp;
	FileLock* local_lock;
	bool local_use_xml;
	priv_state priv;
	ClassAd* eventAd = NULL;

	if (is_global_event) {
		local_fp = global_fp;
		local_lock = global_lock;
		local_use_xml = param_boolean("EVENT_LOG_USE_XML",false);
		priv = set_condor_priv();
	} else {
		local_fp = fp;
		local_lock = lock;
		local_use_xml = use_xml;
		priv = set_user_priv();
	}

	local_lock->obtain (WRITE_LOCK);
	fseek (local_fp, 0, SEEK_END);

		// rotate the global event log if it is too big
	if ( is_global_event ) {
		if ( handleGlobalLogRotation() ) {
				// if we rotated the log, we have a new fp and lock
			local_fp = global_fp;
			local_lock = global_lock;
		}
	}

	if( local_use_xml ) {
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
			if (fprintf (local_fp, adXML.GetCStr()) < 0) {
				success = FALSE;
			} else {
				success = TRUE;
			}
		}
	} else {
		success = event->putEvent (local_fp);
		if (!success) {
			fputc ('\n', local_fp);
		}
		if (fprintf (local_fp, SynchDelimiter) < 0) {
			success = FALSE;
		}
	}

	if ( eventAd ) {
		delete eventAd;
	}		

	if ( fflush(local_fp) != 0 ) {
		dprintf( D_ALWAYS, "fflush() failed in UserLog::doWriteEvent - "
				"errno %d (%s)\n", errno, strerror(errno) );
		// Note:  should we set success to false here?
	}

	// Now that we have flushed the stdio stream, sync to disk
	// *before* we release our write lock!
	// For now, for performance, do not sync the global event log.
	if ( is_global_event == false ) {
		if ( fsync( fileno( local_fp ) ) != 0 ) {
			dprintf( D_ALWAYS, "fsync() failed in UserLog::writeEvent - "
					"errno %d (%s)\n", errno, strerror(errno) );
			// Note:  should we set success to false here?
		}
	}
	local_lock->release ();
	set_priv( priv );
	return success;
}



// Return FALSE(0) on error, TRUE(1) on goodness
int UserLog::
writeEvent (ULogEvent *event, ClassAd *param_jobad)
{
	// the the log is not initialized, don't bother --- just return OK
	if (!fp && !global_fp) {
		return TRUE;
	}
	
	// make certain some parameters we will need are initialized
	if (!event) {
		return FALSE;
	}
	if (fp) {
		if (!lock) return FALSE;
	}
	if (global_fp) {
		if (!global_lock) return FALSE;
	}

	// fill in event context
	event->cluster = cluster;
	event->proc = proc;
	event->subproc = subproc;
	event->setGlobalJobId(m_gjid);
	
	// write global event
	if ( write_global_log && global_fp && 
		 doWriteEvent(event,true,param_jobad)==FALSE ) 
	{
		return FALSE;
	}

	char *attrsToWrite = param("EVENT_LOG_JOB_AD_INFORMATION_ATTRS");
	if ( write_global_log && global_fp && attrsToWrite ) {
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
	if ( write_user_log && fp && doWriteEvent(event,false,param_jobad)==FALSE ) {
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

// ReadUserLog class

ReadUserLog::
ReadUserLog (const char * filename)
{
	clear();

    if (!initialize(filename)) {
		dprintf(D_ALWAYS, "Failed to open %s\n", filename);
    }
}


bool ReadUserLog::
initialize (const char *filename)
{	
	// Note: For whatever reason, we obtain a WRITE lock in method
	// readEvent.  On Linux, if the file is opened O_RDONLY, then a
	// WRITE_LOCK never blocks.  Thus open the file RDWR so the
	// WRITE_LOCK below works correctly.
	//  
	// NOTE: we tried changing this to O_READONLY once and things
	// stopped working right, so don't try it again, smarty-pants!
	if( (_fd = safe_open_wrapper( filename, O_RDWR, 0 )) == -1 ) {
		return FALSE;
	}
	if ((_fp = fdopen (_fd, "r")) == NULL) {
		releaseResources();
	    return FALSE;
	}

	// prepare to lock the file
	if ( param_boolean("ENABLE_USERLOG_LOCKING",true) ) {
    	lock = new FileLock( _fd, _fp, filename );
	} else {
		lock = new FileLock( -1 );
	}
	if( !lock ) {
		releaseResources();
		return FALSE;
	}

	if( !determineLogType() ) {
		releaseResources();
		return FALSE;
	}

	return true;
}

bool ReadUserLog::
determineLogType()
{
	// now determine if the log file is XML and skip over the header (if
	// there is one) if it is XML

	// we obtain a write lock here not because we want to write
	// anything, but because we want to ensure we don't read
	// mid-way through someone else's write
	Lock();

	// store file position so we can rewind to this location
	long filepos = ftell(_fp);
	if( filepos < 0 ) {
		dprintf(D_ALWAYS, "ftell failed in ReadUserLog::determineLogType\n");
		Unlock();
		return FALSE;
	}

	char afterangle;
	int scanf_result = fscanf(_fp, " <%c", &afterangle);
	if( scanf_result == EOF ) {
		// Format is unknown.
		log_type = LOG_TYPE_UNKNOWN;

	} else if( scanf_result > 0 ) {

		setIsXMLLog(TRUE);

		if( !skipXMLHeader(afterangle, filepos) ) {
			log_type = LOG_TYPE_UNKNOWN;
			Unlock();
		    return FALSE;
		}

	} else {
		// the first non whitespace char is not <, so this doesn't look like
		// XML; go back to the beginning and take another look
		if( fseek(_fp, filepos, SEEK_SET) )	{
			dprintf(D_ALWAYS, "fseek failed in ReadUserLog::determineLogType");
			Unlock();
			return FALSE;
		}

		int nothing;
		if( fscanf(_fp, " %d", &nothing) > 0 ) {

			setIsOldLog(TRUE);

			if( fseek(_fp, filepos, SEEK_SET) )	{
				dprintf(D_ALWAYS,
						"fseek failed in ReadUserLog::determineLogType");
				Unlock();
				return FALSE;
			}
		} else {
			// what sort of log is this!
			dprintf(D_ALWAYS, "Error, apparently invalid user log file\n");
			if( fseek(_fp, filepos, SEEK_SET) )	{
				dprintf(D_ALWAYS,
						"fseek failed in ReadUserLog::determineLogType");
				Unlock();
				return FALSE;
			}
			Unlock();
			return FALSE;
		}
	}

	Unlock();

	return TRUE;
}

bool ReadUserLog::
skipXMLHeader(char afterangle, long filepos)
{
	// now figure out if there is a header, and if so, advance _fp past
	// it - this is really ugly
	int nextchar = afterangle;
	if( nextchar == '?' || nextchar == '!' ) {
		// we're probably in the document prolog
		while( nextchar == '?' || nextchar == '!' ) {
			// skip until we get out of this tag
			nextchar = fgetc(_fp);
			while( nextchar != EOF && nextchar != '>' ) {
				nextchar = fgetc(_fp);
			}

			if( nextchar == EOF ) {
				return FALSE;
			}

			// skip until we get to the next tag, saving our location as
			// we go so we can skip back two chars later
			while( nextchar != EOF && nextchar != '<' ) {
				filepos = ftell(_fp);
				nextchar = fgetc(_fp);
			}
			if( nextchar == EOF ) {
				return FALSE;
			}
			nextchar = fgetc(_fp);
		}

		// now we are in a tag like <[^?!]*>, so go back two chars and
		// we're all set
		if( fseek(_fp, filepos, SEEK_SET) )	{
			dprintf(D_ALWAYS, "fseek failed in ReadUserLog::skipXMLHeader");
			return FALSE;
		}
	} else {
		// there was no prolog, so go back to the beginning
		if( fseek(_fp, filepos, SEEK_SET) )	{
			dprintf(D_ALWAYS, "fseek failed in ReadUserLog::skipXMLHeader");
			return FALSE;
		}
	}			

	return TRUE;
}

ULogEventOutcome ReadUserLog::
readEvent (ULogEvent *& event)
{
	if ( !_fp ) {
		return ULOG_NO_EVENT;
	}

	if( log_type == LOG_TYPE_UNKNOWN ) {
	    if( !determineLogType() ) {
			dprintf(D_ALWAYS, "ReadUserLog::determineLogType failed");
			return ULOG_RD_ERROR;
		}
	}

	if( log_type == LOG_TYPE_XML ) {
		return readEventXML(event);
	} else if( log_type == LOG_TYPE_OLD ) {
		return readEventOld(event);
	} else {
		return ULOG_NO_EVENT;
	}
}	

ULogEventOutcome ReadUserLog::
readEventXML(ULogEvent *& event)
{
	ClassAdXMLParser xmlp;

	// we obtain a write lock here not because we want to write
	// anything, but because we want to ensure we don't read
	// mid-way through someone else's write
	Lock();

	// store file position so that if we are unable to read the event, we can
	// rewind to this location
  	long     filepos;
  	if (!_fp || ((filepos = ftell(_fp)) == -1L))
  	{
  		Unlock();
		event = NULL;
  		return ULOG_UNK_ERROR;
  	}

	ClassAd* eventad;
	eventad = xmlp.ParseClassAd(_fp);

	Unlock();

	if( !eventad ) {
		// we don't have the full event in the stream yet; restore file
		// position and return
		if( fseek(_fp, filepos, SEEK_SET) )	{
			dprintf(D_ALWAYS, "fseek() failed in ReadUserLog::readEvent");
			return ULOG_UNK_ERROR;
		}
		clearerr(_fp);
		event = NULL;
		return ULOG_NO_EVENT;
	}

	int enmbr;
	if( !eventad->LookupInteger("EventTypeNumber", enmbr) ) {
		event = NULL;
		delete eventad;
		return ULOG_NO_EVENT;
	}

	if( !(event = instantiateEvent((ULogEventNumber) enmbr)) ) {
		event = NULL;
		delete eventad;
		return ULOG_UNK_ERROR;
	}
	
	event->initFromClassAd(eventad);	

	delete eventad;
	return ULOG_OK;
}

ULogEventOutcome ReadUserLog::
readEventOld(ULogEvent *& event)
{
	long   filepos;
	int    eventnumber;
	int    retval1, retval2;

	// we obtain a write lock here not because we want to write
	// anything, but because we want to ensure we don't read
	// mid-way through someone else's write
	if (!is_locked) {
		lock->obtain( WRITE_LOCK );
	}

	// store file position so that if we are unable to read the event, we can
	// rewind to this location
	if (!_fp || ((filepos = ftell(_fp)) == -1L))
	{
		dprintf( D_FULLDEBUG, "ReadUserLog: invalid _fp, or ftell() failed\n" );
		if (!is_locked) {
			lock->release();
		}
		return ULOG_UNK_ERROR;
	}

	retval1 = fscanf (_fp, "%d", &eventnumber);

	// so we don't dump core if the above fscanf failed
	if (retval1 != 1) {
		eventnumber = 1;
		// check for end of file -- why this is needed has been
		// lost, but it was removed once and everything went to
		// hell, so don't touch it...
			// Note: this is needed because if this method is called and
			// you're at the end of the file, fscanf returns EOF (-1) and
			// you get here.  If you're at EOF you had better bail out...
			// (This is not uncommon -- any time you try to read an event
			// and there aren't any events to read you get here.)
			// If fscanf returns 0, you're probably *really* in trouble.
			// wenger 2004-10-07.
		if( feof( _fp ) ) {
			event = NULL;  // To prevent FMR: Free memory read
			clearerr( _fp );
			if( !is_locked ) {
				lock->release();
			}
			return ULOG_NO_EVENT;
		}
		dprintf( D_FULLDEBUG, "ReadUserLog: error (not EOF) reading "
					"event number\n" );
	}

	// allocate event object; check if allocated successfully
	event = instantiateEvent ((ULogEventNumber) eventnumber);
	if (!event) 
	{
		dprintf( D_FULLDEBUG, "ReadUserLog: unable to instantiate event\n" );
		if (!is_locked) {
			lock->release();
		}
		return ULOG_UNK_ERROR;
	}

	// read event from file; check for result
	retval2 = event->getEvent (_fp);
		
	// check if error in reading event
	if (!retval1 || !retval2)
	{	
		dprintf( D_FULLDEBUG, "ReadUserLog: error reading event; re-trying\n" );

		// we could end up here if file locking did not work for
		// whatever reason (usual NFS bugs, whatever).  so here
		// try to wait a second until the current partially-written
		// event has benn completely written.  the algorithm is
		// wait a second, rewind to our initial position (in case a
		// buggy getEvent() slurped up more than one event), then
		// again try to synchronize the log
		// 
		// NOTE: this code is important, so don't remove or "fix"
		// it unless you *really* know what you're doing and test it
		// extermely well
		if( !is_locked ) {
			lock->release();
		}
		sleep( 1 );
		if( !is_locked ) {
			lock->obtain( WRITE_LOCK );
		}                             
		if( fseek( _fp, filepos, SEEK_SET)) {
			dprintf( D_ALWAYS, "fseek() failed in %s:%d", __FILE__, __LINE__ );
			if (!is_locked) {
				lock->release();
			}
			return ULOG_UNK_ERROR;
		}
		if( synchronize() )
		{
			// if synchronization was successful, reset file position and ...
			if (fseek (_fp, filepos, SEEK_SET))
			{
				dprintf(D_ALWAYS, "fseek() failed in ReadUserLog::readEvent");
				if (!is_locked) {
					lock->release();
				}
				return ULOG_UNK_ERROR;
			}
			
			// ... attempt to read the event again
			clearerr (_fp);
			int oldeventnumber = eventnumber;
			eventnumber = -1;
			retval1 = fscanf (_fp, "%d", &eventnumber);
			if( retval1 == 1 ) {
			  if( eventnumber != oldeventnumber ) {
			    if( event ) {
			      delete event;
			    }
			    // allocate event object; check if allocated
			    // successfully
			    event =
			      instantiateEvent( (ULogEventNumber)eventnumber );
			    if( !event ) { 
				  dprintf( D_FULLDEBUG, "ReadUserLog: unable to "
				  			"instantiate event\n" );
			      if( !is_locked ) {
					lock->release();
			      }
			      return ULOG_UNK_ERROR;
			    }
			  }
			  retval2 = event->getEvent( _fp );
			}

			// if failed again, we have a parse error
			if (!retval1 != 1 || !retval2)
			{
				dprintf( D_FULLDEBUG, "ReadUserLog: error reading event "
							"on second try\n");
				delete event;
				event = NULL;  // To prevent FMR: Free memory read
				synchronize ();
				if (!is_locked) {
					lock->release();
				}
				return ULOG_RD_ERROR;
			}
			else
			{
			  // finally got the event successfully --
			  // synchronize the log
			  if( synchronize() ) {
			    if( !is_locked ) {
			      lock->release();
			    }
			    return ULOG_OK;
			  }
			  else
			  {
			    // got the event, but could not synchronize!!
			    // treat as incomplete event
				dprintf( D_FULLDEBUG, "ReadUserLog: got event on second try "
						"but synchronize() failed\n");
			    delete event;
			    event = NULL;  // To prevent FMR: Free memory read
			    clearerr( _fp );
			    if( !is_locked ) {
			      lock->release();
			    }
			    return ULOG_NO_EVENT;
			  }
			}
		}
		else
		{
			// if we could not synchronize the log, we don't have the full	
			// event in the stream yet; restore file position and return
			dprintf( D_FULLDEBUG, "ReadUserLog: syncronize() failed\n");
			if (fseek (_fp, filepos, SEEK_SET))
			{
				dprintf(D_ALWAYS, "fseek() failed in ReadUserLog::readEvent");
				if (!is_locked) {
					lock->release();
				}
				return ULOG_UNK_ERROR;
			}
			clearerr (_fp);
			delete event;
			event = NULL;  // To prevent FMR: Free memory read
			if (!is_locked) {
				lock->release();
			}
			return ULOG_NO_EVENT;
		}
	}
	else
	{
		// got the event successfully -- synchronize the log
		if (synchronize ())
		{
			if (!is_locked) {
				lock->release();
			}
			return ULOG_OK;
		}
		else
		{
			// got the event, but could not synchronize!!  treat as incomplete
			// event
			dprintf( D_FULLDEBUG, "ReadUserLog: got event on first try "
					"but synchronize() failed\n");

			delete event;
			event = NULL;  // To prevent FMR: Free memory read
			clearerr (_fp);
			if (!is_locked) {
				lock->release();
			}
			return ULOG_NO_EVENT;
		}
	}

	// will not reach here
	if (!is_locked) {
		lock->release();
	}

	dprintf( D_ALWAYS, "Error: got to the end of "
			"ReadUserLog::readEventOld()\n");

	return ULOG_UNK_ERROR;
}

void ReadUserLog::
Lock()
{
	if ( !is_locked ) {
		is_locked = lock->obtain( WRITE_LOCK );
	}
	ASSERT( is_locked );
}

void ReadUserLog::
Unlock()
{
	if ( is_locked ) {
		is_locked = ! lock->release();
	}
	ASSERT( !is_locked );
}

bool ReadUserLog::
synchronize ()
{
	const int bufSize = 512;
    char buffer[bufSize];
    while( fgets( buffer, bufSize, _fp ) != NULL ) {
		if( strcmp( buffer, SynchDelimiter) == 0 ) {
            return TRUE;
        }
    }
    return FALSE;
}

void ReadUserLog::outputFilePos(const char *pszWhereAmI)
{
	dprintf(D_ALWAYS, "Filepos: %ld, context: %s\n", ftell(_fp), pszWhereAmI);
}

void ReadUserLog::
setIsXMLLog( bool is_xml )
{
	if( is_xml ) {
	    log_type = LOG_TYPE_XML;
	} else {
	    log_type = LOG_TYPE_UNKNOWN;
	}
}

bool ReadUserLog::
getIsXMLLog()
{
	return ( log_type == LOG_TYPE_XML );
}

void ReadUserLog::
setIsOldLog( bool is_old )
{
	if( is_old ) {
	    log_type = LOG_TYPE_OLD;
	} else {
	    log_type = LOG_TYPE_UNKNOWN;
	}
}

bool ReadUserLog::
getIsOldLog()
{
	return ( log_type == LOG_TYPE_OLD );
}

void ReadUserLog::
clear()
{
    _fd = -1;
	_fp = NULL;
	lock = NULL;
	is_locked = FALSE;
	log_type = LOG_TYPE_UNKNOWN;
}

void ReadUserLog::
releaseResources()
{
    if (_fp) {
		fclose(_fp);
		_fp = NULL;
		_fd = -1;
	}

	if (_fd != -1) {
	    close(_fd);
		_fd = -1;
	}

	if (is_locked) {
		lock->release();
	}

	delete lock;
	lock = NULL;

	log_type = LOG_TYPE_UNKNOWN;
}
