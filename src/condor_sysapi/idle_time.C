/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
#include "condor_config.h"
#include "condor_debug.h"
#include "sysapi.h"
#include "sysapi_externs.h"

#ifdef Darwin
#include <mach/mach.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/hid/IOHIDLib.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Carbon/Carbon.h>
#endif

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
#ifndef Darwin
static time_t utmp_pty_idle_time( time_t now );
static time_t all_pty_idle_time( time_t now );
static time_t dev_idle_time( char *path, time_t now );
static void calc_idle_time_cpp(time_t & m_idle, time_t & m_console_idle);
#endif

/* we must now use the kstat interface to get this information under later
	releases of Solaris */
#if defined(Solaris28) || defined(Solaris29)
static time_t solaris_kbd_idle(void);
static time_t solaris_mouse_idle(void);
#endif

#endif


/* the local function that does the main work */


#if defined(WIN32)

// ThreadInteract allows the calling thread to access the visable,
// interactive desktop in order to set a hook. 
// Win32
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


// Win32
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

// Win32
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
			CURSORINFO cursor_inf;
			cursor_inf.cbSize = sizeof(CURSORINFO);
			if ( ! GetCursorInfo(&cursor_inf) ) {
				dprintf(D_ALWAYS,"GetCursorInfo() failed (err=%li)\n",
					   	GetLastError());
			} else {
				if ( (cursor_inf.ptScreenPos.x != previous_pos.x) || 
					(cursor_inf.ptScreenPos.y != previous_pos.y) ) {
						// the mouse has moved!
						// stash new position
					previous_pos.x = cursor_inf.ptScreenPos.x; 
					previous_pos.y = cursor_inf.ptScreenPos.y; 
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

#elif !defined(Darwin) /* !defined(WIN32) -- all UNIX platforms but OS X*/

/* calc_idle_time fills in user_idle and console_idle with the number
 * of seconds since there has been activity detected on any tty or on
 * just the console, respectively.  therefore, console_idle will always
 * be equal to or greater than user_idle.  think about it.  also, note
 * that user_idle and console_idle are passed by reference.  Also,
 * on some platforms console_idle is always -1 because it cannot reliably
 * be determined.
 */
// Unix
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
#elif defined(Solaris28) || defined(Solaris29)
#include <utmpx.h>
static char *UtmpName = "/etc/utmpx";
static char *AltUtmpName = "/var/adm/utmpx";
#undef UTMP_KIND
#define UTMP_KIND utmpx
#elif defined(HPUX11)
/* actually, we use the xpg4.2 API to get the utmpx interface, not the 
	files/method we normally would use. This prevents 32/64 bit
	incompatibilities in the member fields of the utmpx structure.
	- psilord 01/09/2002
	*/
#include <utmpx.h>
#undef UTMP_KIND
#define UTMP_KIND utmpx
#else
static char *UtmpName = "/etc/utmp";
static char *AltUtmpName = "/var/adm/utmp";
#endif

// Unix
time_t
utmp_pty_idle_time( time_t now )
{
	time_t tty_idle;
	time_t answer = (time_t)INT_MAX;
	static time_t saved_now;
	static time_t saved_idle_answer = -1;

#if !defined(HPUX11)

	/* freading structures directly out of a file is a pretty bad idea
		exemplified by the fact that the elements in the utmp
		structure may change size based upon if you are
		compiling in 32 bit or 64 bit mode, but the byte lengths
		of the fields in the utmp file might not.  So, for
		everyone, but HPUX11, we'll leave it the old fread way,
		but on HPUX11, we'll use the xpg4.2 utmpx API to get
		this information out of the utmpx file. Maybe we should
		figure out the sactioned ways on each platform to do
		this if one exists. -psilord 01/09/2002 */

	FILE *fp;
	struct UTMP_KIND utmp_info;

	if ((fp=fopen(UtmpName,"r")) == NULL) {
		if ((fp=fopen(AltUtmpName,"r")) == NULL) {
			EXCEPT("fopen of \"%s\"", UtmpName);
		}
	}

	while (fread((char *)&utmp_info, sizeof(struct UTMP_KIND), 1, fp)) {
#if defined(AIX) || defined(LINUX) || defined(OSF1) || defined(IRIX65)
		if (utmp_info.ut_type != USER_PROCESS)
#else
			if (utmp_info.ut_name[0] == '\0')
#endif
				continue;
		
		tty_idle = dev_idle_time(utmp_info.ut_line, now);
		answer = MIN(tty_idle, answer);
	}
	fclose(fp);

#else /* hpux 11 */

	/* Here we use the xpg4.2 utmpx interface to process the utmpx entries
		and figure out which ttys are assigned to user processes. */

	struct UTMP_KIND *utmp_info;

	while((utmp_info = getutxent()) != NULL)
	{
		if (utmp_info->ut_type != USER_PROCESS)
			continue;

		tty_idle = dev_idle_time(utmp_info->ut_line, now);
		answer = MIN(tty_idle, answer);
	}

#endif

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

// Unix
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
		Hopefully the performance might not be that bad under linux since
		this stuff is created each and every time this function is called.
		psilord 1/4/2002
	*/
#if defined(LINUX) && (defined(GLIBC22) || defined(GLIBC23))
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
#elif defined( OSF1 ) || defined(Darwin)
#include <sys/types.h>
#elif defined( HPUX )
#include <sys/sysmacros.h>
#elif defined( AIX )
#include <sys/types.h>
#else
#include <sys/mkdev.h>
#endif

// Unix
time_t
dev_idle_time( char *path, time_t now )
{
	struct stat	buf;
	time_t answer;
	#if defined(Solaris28) || defined(Solaris29)
	time_t kstat_answer;
	#endif
	static char pathname[100] = "/dev/";
	static int null_major_device = -1;

	if ( !path || path[0]=='\0' ||
		 strncmp(path,"unix:",5) == 0 ) {
		// we don't have a valid path, or it is
		// a nonuseful/fake path setup by the X server
		return now;
	}

	strcpy( &pathname[5], path );
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
	
	/* ok, just check the device idle time for normal devices using stat() */
	if (stat(pathname,&buf) < 0) {
		if( errno != ENOENT ) {
			dprintf( D_FULLDEBUG, "Error on stat(%s,0x%x), errno = %d(%s)\n",
					 pathname, &buf, errno, strerror(errno) );
		}
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

	/* under solaris2[89], we'll catch when someone asks about specifically
		about a console or mouse device, and specially figure
		them out since we cannot stat() them anymore under
		solaris 8 update 6+ or solaris 9. stat() will return 0
		for the access time no matter what. */

	#if defined(Solaris28) || defined(Solaris29)
	
		/* if I happen not to be dealing with a console device, we better 
			choose what the stat() answer would have been, so initialize this
			to zero for force it when the MAX macro gets used */
		kstat_answer = 0;

		if (strstr(path, "kbd") != NULL)
		{
			kstat_answer = solaris_kbd_idle();
		}

		if (strstr(path, "mouse") != NULL || strstr(path, "consms") != NULL)
		{
			kstat_answer = solaris_mouse_idle();
		}
	
	/* ok, now we'll grab the higher of the two answers. zero for the busted
		stat() or the real value for the kstat(). If they are both zero,
		then, well, someone is using it */
	answer = MAX(answer, kstat_answer);

	#endif

	if( (DebugFlags & D_FULLDEBUG) && (DebugFlags & D_IDLE) ) {
        dprintf( D_IDLE, "%s: %d secs\n", pathname, (int)answer );
	}

	return answer;
}

#else /* here's the OS X version of calc_idle_time */

void calc_idle_time_cpp(time_t * user_idle, time_t * console_idle);
static time_t extract_idle_time(CFMutableDictionaryRef  properties);

/****************************************************************************
 *
 * Function: calc_idle_time_cpp
 * Purpose:  This calculates the idle time on the Mac. It uses the HID 
 *           (Human Interface Device) Manager, which happens to track
 *           the system idle time. It doesn't distinguish user idle and 
 *           console_idle, so they are set to be the same. If you investigate
 *           the HID Manager deeply, you'll notice that it should be possible
 *           to get the idle time of the keyboard and mouse separately. As of
 *           April 2003, that doesn't work, because you can't use the HID 
 *           Manager to query the keyboard.
 ****************************************************************************/
void
calc_idle_time_cpp(time_t * user_idle, time_t * console_idle)
{

    mach_port_t             masterPort           = NULL;
    io_iterator_t           hidObjectIterator    = NULL;
    CFMutableDictionaryRef  hidMatchDictionary   = NULL;
    io_object_t             hidDevice            = NULL;
    
    *user_idle = *console_idle = -1;
    
    if (IOMasterPort(bootstrap_port, &masterPort) != kIOReturnSuccess) {
        dprintf(D_ALWAYS, "IDLE: Couldn't create a master I/O Kit port.\n");
    } else {
        hidMatchDictionary = IOServiceMatching("IOHIDSystem");
        if (IOServiceGetMatchingServices(masterPort, hidMatchDictionary, &hidObjectIterator) != kIOReturnSuccess) {
            dprintf(D_ALWAYS, "IDLE: Can't find IOHIDSystem\n");
        } else if (hidObjectIterator == NULL) {
            dprintf(D_ALWAYS, "IDLE Can't find IOHIDSystem\n");
        } else {
            // Note that IOServiceGetMatchingServices consumes a reference to the dictionary
            // so we don't need to release it. We'll mark it as NULL, so we don't try to reuse it.
            hidMatchDictionary = NULL;

            hidDevice = IOIteratorNext(hidObjectIterator);
                
            CFMutableDictionaryRef  properties = 0;
            if (   IORegistryEntryCreateCFProperties(hidDevice,
                        &properties, kCFAllocatorDefault, kNilOptions) == KERN_SUCCESS
                && properties != 0) {
                
                *user_idle = extract_idle_time(properties);
                *console_idle = *user_idle;
                CFRelease(properties);
            }
            IOObjectRelease(hidDevice);
            IOObjectRelease(hidObjectIterator);    
        }
        if (masterPort) {
            mach_port_deallocate(mach_task_self(), masterPort);
        }
    }
    return;
}

/****************************************************************************
 *
 * Function: extract_idle_time
 * Purpose:  The HID Manager reports the idle time in nanoseconds. I'm not
 *           sure if it really tracks with that kind of percision. Anyway, 
 *           we extract the idle time from the dictionary, and convert it 
 *           into seconds. 
 *
 ****************************************************************************/
static time_t 
extract_idle_time(
    CFMutableDictionaryRef  properties)
{
    time_t    idle_time = -1;
	UInt64  nanoseconds, billion, seconds_64, remainder;
	UInt32  seconds;
    CFTypeRef object = CFDictionaryGetValue(properties, CFSTR(kIOHIDIdleTimeKey));
	CFRetain(object);
 
    if (!object) {
	dprintf(D_ALWAYS, "IDLE: Failed to make CFTypeRef\n");
    } else {
		CFTypeID object_type = CFGetTypeID(object);
		if (object_type == CFNumberGetTypeID()) {
			CFNumberGetValue((CFNumberRef)object, kCFNumberSInt64Type,
							&nanoseconds);
        } else if (object_type == CFDataGetTypeID()) {
            Size num_bytes;
            num_bytes = CFDataGetLength((CFDataRef) object);
            if (num_bytes != 8) {
                dprintf(D_ALWAYS, "IDLE: Idle time is not 8 bytes.\n");
				CFRelease(object);
				return idle_time;
            } else {
				CFDataGetBytes((CFDataRef) object, CFRangeMake(0, 8), 
				((UInt8 *) &nanoseconds));
			}
        } else {
            dprintf(D_ALWAYS, "IDLE: Idle time didnt match CFDataGetTypeID.\n");
			CFRelease(object);
			return idle_time;
		}	
		billion = U64SetU(1000000000);
		seconds_64 = U64Divide(nanoseconds, billion, &remainder);
		seconds = U32SetU(seconds_64);
		idle_time = seconds;
    }
	// CFRelease seems to be hip with taking a null object. This seems
	// strange to me, but hey, at least I thought about it
	CFRelease(object);
    return idle_time;
}

#endif  /* the end of the Mac OS X code, and the  */

/* under solaris 8 with update 6 and later, and solaris 9, use the kstat 
	interface to determine the console and mouse idle times. stat() on the
	actual kbd or mouse device no longer returns(by design according to 
	solaris) the access time. These are localized functions static to this
	object file. */
#if defined(Solaris28) || defined(Solaris29)
static time_t solaris_kbd_idle(void)
{
	kstat_ctl_t     *kc = NULL;  /* libkstat cookie */ 
	kstat_t         *kbd_ksp = NULL; 
	void *p = NULL;
	time_t answer;

	/* open the kstat device */
	if ((kc = kstat_open()) == NULL) {

		dprintf(D_FULLDEBUG, 
			"solaris_kbd_idle(): Can't open /dev/kstat: %d(%s)\n",
			errno, strerror(errno));

		dprintf(D_FULLDEBUG, "solaris_kbd_idle(): assuming zero idle time!\n");

		return (time_t)0;
	}

	/* find the keyboard device */
	kbd_ksp = kstat_lookup(kc, "conskbd", 0, "activity");
	if (kbd_ksp == NULL) {
		
		if (errno == ENOMEM)
		{
			/* NOTE: This is an educated guess for what a Solaris machine which 
				doesn't support the lookup I'm asking gives back as an errno
				here. Since this is a possibility on solaris 8 machines
				BEFORE Update 6, I have to notice it, and then silently
				ignore it, because the stat() that was done previously on the
				device will have worked as expected. I suppose it might
				be possible to have this error happen on a machine where
				this code should have worked, in which case sorry.
				*/
			if (kstat_close(kc) != 0)
			{
				/* XXX I hope there isn't a resource leak if this ever 
					happens */
				dprintf(D_FULLDEBUG, 
					"solaris_kbd_idle(): Can't close /dev/kstat: %d(%s)\n",
					errno, strerror(errno));
			}

			return (time_t)0;
		}

		dprintf(D_FULLDEBUG, 
			"solaris_kbd_idle(): Keyboard init failed: %d(%s)\n",
			errno, strerror(errno));

		dprintf(D_FULLDEBUG, "solaris_kbd_idle(): assuming zero idle time!\n");

		if (kstat_close(kc) != 0)
		{
			/* XXX I hope there isn't a resource leak if this ever happens */
	
			dprintf(D_FULLDEBUG, 
				"solaris_kbd_idle(): Can't close /dev/kstat: %d(%s)\n",
				errno, strerror(errno));
		}

		return (time_t)0;
	}

	/* get the idle time from the keyboard device */
	if (kstat_read(kc, kbd_ksp, NULL) == -1 ||
		(p = kstat_data_lookup(kbd_ksp, "idle_sec")) == NULL)
	{
		dprintf(D_FULLDEBUG, 
			"solaris_kbd_idle(): Can't get idle time from /dev/kstat: %d(%s)\n",
			errno, strerror(errno));

		if (kstat_close(kc) != 0)
		{
			/* XXX I hope there isn't a resource leak if this ever happens */
	
			dprintf(D_FULLDEBUG, 
				"solaris_kbd_idle(): Can't close /dev/kstat: %d(%s)\n",
				errno, strerror(errno));
		}

		return (time_t)0;
	}

	/* ok sanity check it, and look at the value BEFORE I do a kstat_close */
	answer = ((kstat_named_t *)p)->value.l;
	if (answer < (time_t)0)
	{
		dprintf(D_FULLDEBUG,
			"solaris_kbd_idle(): WARNING! Fixing a negative idle time!\n");
			
		answer = 0;
	}

	/* close kstat */
	if (kstat_close(kc) != 0)
	{
		/* XXX I hope there isn't a resource leak if this ever happens */

		dprintf(D_FULLDEBUG, 
			"solaris_kbd_idle(): Can't close /dev/kstat: %d(%s)\n",
			errno, strerror(errno));
	}
	
	return answer;
}

static time_t solaris_mouse_idle(void)
{
	kstat_ctl_t     *kc = NULL;  /* libkstat cookie */ 
	kstat_t         *ms_ksp = NULL; 
	time_t answer;
	void *p = NULL;

	/* open kstat */
	if ((kc = kstat_open()) == NULL) {

		dprintf(D_ALWAYS, 
			"solaris_mouse_idle(): Can't open /dev/kstat: %d(%s)\n",
			errno, strerror(errno));

		dprintf(D_FULLDEBUG, 
			"solaris_mouse_idle(): assuming zero idle time!\n");

		return (time_t)0;
	}

	/* find the mouse device */
	ms_ksp = kstat_lookup(kc, "consms", 0, "activity");
	if (ms_ksp == NULL) {

		if (errno == ENOMEM)
		{
			/* NOTE: This is an educated guess for what a Solaris machine which 
				doesn't support the lookup I'm asking gives back as an errno
				here. Since this is a possibility on solaris 8 machines
				BEFORE Update 6, I have to notice it, and then silently
				ignore it, because the stat() that was done previously on the
				device will have worked as expected. I suppose it might
				be possible to have this error happen on a machine where
				this code should have worked, in which case sorry.
				*/
			if (kstat_close(kc) != 0)
			{
				/* XXX I hope there isn't a resource leak if this ever 
					happens */
				dprintf(D_FULLDEBUG, 
					"solaris_mouse_idle(): Can't close /dev/kstat: %d(%s)\n",
					errno, strerror(errno));
			}

			return (time_t)0;
		}

		dprintf(D_FULLDEBUG, 
			"solaris_mouse_idle(): Mouse init failed: %d(%s)\n",
			errno, strerror(errno));

		dprintf(D_FULLDEBUG, 
			"solaris_mouse_idle(): assuming zero idle time!\n");

		if (kstat_close(kc) != 0)
		{
			/* XXX I hope there isn't a resource leak if this ever happens */

			dprintf(D_FULLDEBUG,
				"solaris_mouse_idle(): Can't close /dev/kstat: %d(%s)\n",
				errno, strerror(errno));
		}

		return (time_t)0;
	}

	/* get the idle time from the mouse device */
	if (kstat_read(kc, ms_ksp, NULL) == -1 ||
		(p = kstat_data_lookup(ms_ksp, "idle_sec")) == NULL)
	{
		dprintf(D_FULLDEBUG,
			"solaris_mouse_idle(): Can't get idle time from /dev/kstat: "
				"%d(%s)\n", errno, strerror(errno));

		if (kstat_close(kc) != 0)
		{
			/* XXX I hope there isn't a resource leak if this ever happens */

			dprintf(D_FULLDEBUG, 
				"solaris_mouse_idle(): Can't close /dev/kstat: %d(%s)\n",
				errno, strerror(errno));
		}

		return (time_t)0;
	}

	/* ok, sanity check the result */
	answer = ((kstat_named_t *)p)->value.l;
	if (answer < (time_t)0)
	{
		dprintf(D_FULLDEBUG,
			"solaris_mouse_idle(): WARNING! Fixing a negative idle time!\n");
			
		answer = 0;
	}

	/* close kstat */
	if (kstat_close(kc) != 0)
	{
		/* XXX I hope there isn't a resource leak if this ever happens */

		dprintf(D_FULLDEBUG,
			"solaris_mouse_idle(): Can't close /dev/kstat: %d(%s)\n",
			errno, strerror(errno));
	}

	return answer;
}

#endif


/* ok, the purpose of this code is to create an interface that is a C linkage
 out of this file. This will get a little ugly. The sysapi entry points
 are C linkage */

BEGIN_C_DECLS

void
sysapi_idle_time_raw(time_t *m_idle, time_t *m_console_idle)
{


	sysapi_internal_reconfig();

#if( !defined( WIN32 ) && !defined( Darwin ) )
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
