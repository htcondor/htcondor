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
#include "condor_config.h"
#include "condor_debug.h"
#include "sysapi.h"
#include "sysapi_externs.h"

/* define some static functions */
#if defined(WIN32)
static BOOL ThreadInteract(HDESK * hdesk, HWINSTA * hwinsta);
static DWORD WINAPI message_loop_thread(void *);
static void calc_idle_time_cpp(time_t * m_idle, time_t * m_console_idle);
extern int WINAPI KBInitialize(void);
extern int WINAPI KBShutdown(void);
extern int WINAPI KBQuery(void);
#else /* Not defined WIN32 */
#include "string_list.h"
static time_t utmp_pty_idle_time( time_t now );
static time_t all_pty_idle_time( time_t now );
static time_t dev_idle_time( char *path, time_t now );
static void calc_idle_time_cpp(time_t & m_idle, time_t & m_console_idle);
#endif

/* the local function that does the main work */


#if defined(WIN32)

// ThreadInteract allows the calling thread to access the visable,
// interactive desktop in order to set a hook. 
BOOL ThreadInteract(HDESK * hdesk_input, HWINSTA * hwinsta_input)   
{      
	HDESK   hdeskTest;
	HDESK	hdesk;
	HWINSTA	hwinsta;
	
	*hdesk_input = NULL;
	*hwinsta_input = NULL;

	// Obtain a handle to WinSta0 - service must be running
	// in the LocalSystem account or this will fail!!!!!     
	hwinsta = OpenWindowStation("winsta0", FALSE,
							  WINSTA_ACCESSCLIPBOARD   |
							  WINSTA_ACCESSGLOBALATOMS |
							  WINSTA_CREATEDESKTOP     |
							  WINSTA_ENUMDESKTOPS      |
							  WINSTA_ENUMERATE         |
							  WINSTA_EXITWINDOWS       |
							  WINSTA_READATTRIBUTES    |
							  WINSTA_READSCREEN        |
							  WINSTA_WRITEATTRIBUTES);

	if (hwinsta == NULL)         
	  return FALSE; 

	// Set the windowstation to be winsta0     
	if (!SetProcessWindowStation(hwinsta))         
	  return FALSE;      

	// Get the desktop     
	hdeskTest = GetThreadDesktop(GetCurrentThreadId());
	if (hdeskTest == NULL)         
	  return FALSE;     

	// Get the default desktop on winsta0      
	hdesk = OpenDesktop("default", 0, FALSE,               
						DESKTOP_HOOKCONTROL |
						DESKTOP_READOBJECTS |
						DESKTOP_SWITCHDESKTOP |
						DESKTOP_WRITEOBJECTS);   

	if (hdesk == NULL)
	   return FALSE;   

	// Set the desktop to be "default"   
	if (!SetThreadDesktop(hdesk))           
	  return FALSE;   

	*hdesk_input = hdesk;
	*hwinsta_input = hwinsta;

	return TRUE;
}


static DWORD WINAPI
message_loop_thread(void *foo)
{
	MSG msg;
	HDESK hdesk;
	HWINSTA hwinsta;

	// first allow this thread access to the interactive(visable) desktop
	while ( !ThreadInteract(&hdesk,&hwinsta) ) {
		// failed to get access to the desktop!!!  Perhaps we are still
		// at the login screen....
		dprintf(D_ALWAYS,
			"Failed to access the interactive desktop; will try later\n");
		// now sleep for 2 minutes and try again
		sleep(120);
	}

	// tell our DLL to hook onto WH_KEYBOARD messages
	if ( !KBInitialize() ) {
		dprintf(D_ALWAYS,"ERROR: could not initialize kbdd DLL\n");
		return 0;
	}

	while (GetMessage( &msg, NULL, 0, 0 ) ) {
		TranslateMessage( &msg );
		DispatchMessage( &msg );	
	}

	// TODO: not exactly certain what we should do
	//		when GetMessage returns NULL.  at this
	//		stage I won't worry about it, because
	//		it is likely we are about to get killed
	//		anyhow...
	//		I guess we should unhook since things could
	//		get sluggish without a message loop....
	dprintf(D_ALWAYS,"WARNING: GetMessage() returned NULL\n");

	KBShutdown();	// will unhook WH_KEYBOARD messages

	// Close the windowstation and desktop handles   
	CloseWindowStation(hwinsta);
	CloseDesktop(hdesk);

	return 0;
}

void
calc_idle_time_cpp( time_t * user_idle, time_t * console_idle)
{
	static HANDLE threadHandle = NULL;
	static DWORD threadID = -1;
	static POINT previous_pos = { 0, 0 };	// stored position of cursor
	time_t now = time( 0 );

	if (threadHandle == NULL) {

		// First time calc_idle_time has been called.

		// Start a thread to act as a message pump.  We need this
		// because down because SetWindowsHookEx forwards messages 
		// to the same thread which performs the hook.  So, whatever
		// thread calls SetWindowsHookEx also needs to have a message
		// pump.
		threadHandle = CreateThread(NULL, 0, message_loop_thread, 
									NULL, 0, &threadID);
		if (threadHandle == NULL) {
			dprintf(D_ALWAYS, "failed to create message loop thread, errno = %d\n",
					GetLastError());
			*user_idle = -1;
			*console_idle = -1;
		}		
	} else {

		// Not the first time calc_idle_time has been called

		if ( KBQuery() ) {
			// a keypress was detected
			_sysapi_last_x_event = now;
		} else {
			// no keypress detected, test if mouse moved
			POINT current_pos;
			if ( ! GetCursorPos(&current_pos) ) {
				dprintf(D_ALWAYS,"GetCursorPos failed\n");
			} else {
				if ( (current_pos.x != previous_pos.x) || 
					(current_pos.y != previous_pos.y) ) {
					// the mouse has moved!
					previous_pos = current_pos;	// stash new position
					_sysapi_last_x_event = now;
				}
			}
		}

	}

	*user_idle = now - _sysapi_last_x_event;
	*console_idle = *user_idle;

	dprintf( D_IDLE, "Idle Time: user= %d , console= %d seconds\n",
			 *user_idle, *console_idle );
	return;
}

#else /* !defined(WIN32) */

/* calc_idle_time fills in user_idle and console_idle with the number
 * of seconds since there has been activity detected on any tty or on
 * just the console, respectively.  therefore, console_idle will always
 * be equal to or greater than user_idle.  think about it.  also, note
 * that user_idle and console_idle are passed by reference.  Also,
 * on some platforms console_idle is always -1 because it cannot reliably
 * be determined.
 */
void
calc_idle_time_cpp( time_t & m_idle, time_t & m_console_idle )
{
	time_t tty_idle;
	time_t now = time( 0 );
	char* tmp;

		// Find idle time from ptys/ttys.  See if we should trust
		// utmp.  If so, only stat the devices that utmp says are
		// associated with active logins.  If not, stat /dev/tty* and
		// /dev/pty* to make sure we get it right.

	if (_sysapi_startd_has_bad_utmp == TRUE) {
		m_idle = all_pty_idle_time( now );
	} else {
		m_idle = utmp_pty_idle_time( now );
	}
		
		// Now, if CONSOLE_DEVICES is defined in the config file, stat
		// those devices and report console_idle.  If it's not there,
		// console_idle is -1.
	m_console_idle = -1;  // initialize
	if( _sysapi_console_devices ) {
		_sysapi_console_devices->rewind();
		while( (tmp = _sysapi_console_devices->next()) ) {
			tty_idle = dev_idle_time( tmp, now );
			m_idle = MIN( tty_idle, m_idle );
			if( m_console_idle == -1 ) {
				m_console_idle = tty_idle;
			} else {
				m_console_idle = MIN( tty_idle, m_console_idle );
			}
		}
	}

		// If we have the kbdd running, last_x_event will be set to
		// something useful, if not, it's just 0, in which case
		// user_idle will certainly be less than now.
	m_idle = MIN( now - _sysapi_last_x_event, m_idle );

		// If last_x_event != 0, then condor_kbdd told us someone did
		// something on the console, but always believe a /dev device
		// over the kbdd, so if console_idle is set, leave it alone.
	if( (m_console_idle == -1) && (_sysapi_last_x_event != 0) ) {
		m_console_idle = now - _sysapi_last_x_event;
	}

	if( (DebugFlags & D_IDLE) && (DebugFlags & D_FULLDEBUG) ) {
		dprintf( D_IDLE, "Idle Time: user= %d , console= %d seconds\n", 
				 (int)m_idle, (int)m_console_idle );
	}
	return;
}

#include <utmp.h>
#define UTMP_KIND utmp

#if defined(OSF1)
static char *UtmpName = "/var/adm/utmp";
static char *AltUtmpName = "/etc/utmp";
#elif defined(LINUX)
static char *UtmpName = "/var/run/utmp";
static char *AltUtmpName = "/var/adm/utmp";
#elif defined(Solaris28)
#include <utmpx.h>
static char *UtmpName = "/etc/utmpx";
static char *AltUtmpName = "/var/adm/utmpx";
#undef UTMP_KIND
#define UTMP_KIND utmpx
#else
static char *UtmpName = "/etc/utmp";
static char *AltUtmpName = "/var/adm/utmp";
#endif

time_t
utmp_pty_idle_time( time_t now )
{
	FILE *fp;
	time_t tty_idle;
	time_t answer = (time_t)INT_MAX;
	static time_t saved_now;
	static time_t saved_idle_answer = -1;
	struct UTMP_KIND utmp_info;

	if ((fp=fopen(UtmpName,"r")) == NULL) {
		if ((fp=fopen(AltUtmpName,"r")) == NULL) {
			EXCEPT("fopen of \"%s\"", UtmpName);
		}
	}

	while (fread((char *)&utmp_info, sizeof(struct UTMP_KIND), 1, fp)) {
#if defined(AIX31) || defined(AIX32) || defined(IRIX331) || defined(IRIX53) || defined(LINUX) || defined(OSF1) || defined(IRIX62) || defined(IRIX65)
		if (utmp_info.ut_type != USER_PROCESS)
#else
			if (utmp_info.ut_name[0] == '\0')
#endif
				continue;
		
		tty_idle = dev_idle_time(utmp_info.ut_line, now);
		answer = MIN(tty_idle, answer);
	}
	fclose(fp);

	/* Here we check to see if we are about to return INT_MAX.  If so,
	 * we recompute via the last pty access we knew about.  -Todd, 2/97 */
	if ( (answer == INT_MAX) && ( saved_idle_answer != -1 ) ) {
		answer = (now - saved_now) + saved_idle_answer;
		if ( answer < 0 )
			answer = 0; /* someone messed with the system date */
	} else {
		if ( answer != INT_MAX ) {
			/* here we are returning an answer we discovered; save it */
			saved_idle_answer = answer;
			saved_now = now;
		}
	}

	return answer;
}

#include "directory.h"

time_t
all_pty_idle_time( time_t now )
{
	char	*f;

	static Directory *dev = NULL;
	static Directory *dev_pts = NULL;
	static bool checked_dev_pts = false;
	time_t	idle_time;
	time_t	answer = (time_t)INT_MAX;
	struct stat	statbuf;

	if( ! checked_dev_pts ) {
		if( stat("/dev/pts", &statbuf) >= 0 ) {
				// "/dev/pts" exists, let's just make sure it's a
				// directory and then we know we're in good shape.
			if( S_ISDIR(statbuf.st_mode) ) {
				dev_pts = new Directory( "/dev/pts" );
			}
		}
		checked_dev_pts = true;
	}

	if( !dev ) {
#if defined(Solaris)
		dev = new Directory( "/devices/pseudo" );
#else
		dev = new Directory( "/dev" );
#endif
	}

	for( dev->Rewind();  (f = (char*)dev->Next()); ) {
#if defined(Solaris)
		if( strncmp("pts",f,3) == MATCH ) 
#else
		if( strncmp("tty",f,3) == MATCH || strncmp("pty",f,3) == MATCH ) 
#endif
		{
			idle_time = dev_idle_time( f, now );
			if( idle_time < answer ) {
				answer = idle_time;
			}
		}
	}

		// Now, if there's a /dev/pts, search all the devices in there.  
	if( dev_pts ) {
		char pathname[100];
		for( dev_pts->Rewind();  (f = (char*)dev_pts->Next()); ) {
			sprintf( pathname, "pts/%s", f );
			idle_time = dev_idle_time( pathname, now );
			if( idle_time < answer ) {
				answer = idle_time;
			}
		}
	}	

	/* Under linux kernel 2.4, the /dev directory is dynamic. This means tty 
		entries come and go depending upon the number of users actually logged
		in.  If we keep the Directory object static, then we will never know or
		incorrectly check ttys that might or might not exist. 
		Hopefully the performance might not be that bad under linux because
		this stuff is created each and every time this function is called.
		psilord 1/4/2002
	*/
#if defined(LINUX) && defined(GLIBC22)
	if (dev != NULL)
	{
		delete dev;
		dev = NULL;
	}

	if (checked_dev_pts == true)
	{
		if (dev_pts != NULL)
		{
			delete dev_pts;
			dev_pts = NULL;
		}
		checked_dev_pts = false;
	}
#endif

	return answer;
}

#ifdef LINUX
#include <sys/sysmacros.h>  /* needed for major() below */
#elif defined( OSF1 )
#include <sys/types.h>
#elif defined( HPUX )
#include <sys/sysmacros.h>
#else
#include <sys/mkdev.h>
#endif

time_t
dev_idle_time( char *path, time_t now )
{
	struct stat	buf;
	time_t answer;
	static char pathname[100] = "/dev/";
	static int null_major_device = -1;

	if ( !path || path[0]=='\0' ||
		 strncmp(path,"unix:",5) == 0 ) {
		// we don't have a valid path, or it is
		// a bullshit path setup by the X server
		return now;
	}

	if ( null_major_device == -1 ) {
		// get the major device number of /dev/null so
		// we can ignore any device that shares that
		// major device number (things like /dev/null,
		// /dev/kmem, etc).
		null_major_device = -2;	// so we don't try again
		if ( stat("/dev/null",&buf) < 0 ) {
			dprintf(D_ALWAYS,"Cannot stat /dev/null\n");
		} else {
			// we were able to stat /dev/null, stash dev num
			if ( !S_ISREG(buf.st_mode) && !S_ISDIR(buf.st_mode) &&
				 !S_ISLNK(buf.st_mode) ) {
				null_major_device = major(buf.st_rdev);
				dprintf(D_FULLDEBUG,"/dev/null major dev num is %d\n",
								null_major_device);
			}
		}
	}
	
	strcpy( &pathname[5], path );
	if (stat(pathname,&buf) < 0) {
		dprintf( D_FULLDEBUG, "Error on stat(%s,0x%x), errno = %d(%s)\n",
				 pathname, &buf, errno, strerror(errno) );
		buf.st_atime = 0;
	}
	if ( null_major_device > -1 && null_major_device == major(buf.st_rdev) ) {
		// this device is related to /dev/null, it should not count
		buf.st_atime = 0;
	}
	answer = now - buf.st_atime;
	if( buf.st_atime > now ) {
		answer = 0;
	}

	if( (DebugFlags & D_FULLDEBUG) && (DebugFlags & D_IDLE) ) {
        dprintf( D_IDLE, "%s: %d secs\n", pathname, (int)answer );
	}

	return answer;
}

#endif /* defined(WIN32) */


/* ok, the purpose of this code is to create an interface that is a C linkage
 out of this file. This will get a little ugly. The sysapi entry points
 are C linkage */

BEGIN_C_DECLS

void
sysapi_idle_time_raw(time_t *m_idle, time_t *m_console_idle)
{


	sysapi_internal_reconfig();

#ifndef WIN32
	time_t m_i, m_c;

	/* here calc_idle_time_cpp expects a reference, so let's give it one */
	calc_idle_time_cpp(m_i, m_c);

	*m_idle = m_i;
	*m_console_idle = m_c;
#else
	calc_idle_time_cpp(m_idle,m_console_idle);
#endif
}

void
sysapi_idle_time(time_t *m_idle, time_t *m_console_idle)
{
	sysapi_internal_reconfig();

	sysapi_idle_time_raw(m_idle, m_console_idle);
}

END_C_DECLS
