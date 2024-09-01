/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "sysapi.h"
#include "sysapi_externs.h"

#if defined(DARWIN)
#include <mach/mach.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/hid/IOHIDLib.h>
#include <CoreFoundation/CoreFoundation.h>
#endif

/* define some static functions */
#if defined(WIN32)
static void calc_idle_time_cpp(time_t * m_idle, time_t * m_console_idle);
#else /* Not defined WIN32 */
#ifndef Darwin
/* This struct hold information about the idle time of the keyboard and mouse */
typedef struct {
	unsigned long num_key_intr;
	unsigned long num_mouse_intr;
	time_t timepoint;
} idle_t;

static time_t utmp_pty_idle_time( time_t now );
static time_t all_pty_idle_time( time_t now );
static time_t dev_idle_time( const char *path, time_t now );
static void calc_idle_time_cpp(time_t & m_idle, time_t & m_console_idle);

#endif

#endif


/* the local function that does the main work */

#if defined(WIN32)

// Win32
void
calc_idle_time_cpp( time_t * user_idle, time_t * console_idle)
{
    time_t now = time( 0 );
	
	*user_idle = now - _sysapi_last_x_event;
	*console_idle = *user_idle;
	
	dprintf( D_IDLE, "Idle Time: user= %d , console= %d seconds\n",
		*user_idle, *console_idle );
	return;
}

#elif !defined(Darwin) /* !defined(WIN32) -- all UNIX platforms but OS X*/

/* delimiting characters between the numbers in the /proc/interrupts file */
#define DELIMS " "
#define BUFFER_SIZE (1024*10)

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

		// Find idle time from ptys/ttys.  See if we should trust
		// utmp.  If so, only stat the devices that utmp says are
		// associated with active logins.  If not, stat /dev/tty* and
		// /dev/pty* to make sure we get it right.

	if (_sysapi_startd_has_bad_utmp) {
		m_idle = all_pty_idle_time( now );
	} else {
		m_idle = utmp_pty_idle_time( now );
	}
		
		// Now, if CONSOLE_DEVICES is defined in the config file, stat
		// those devices and report console_idle.  If it's not there,
		// console_idle is -1.
	m_console_idle = -1;  // initialize
	if( _sysapi_console_devices ) {
		for (auto& tmp: *_sysapi_console_devices) {
			tty_idle = dev_idle_time( tmp.c_str(), now );
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
		// something on the console.
	if ( _sysapi_last_x_event ) {
		if( m_console_idle != -1 ) {
			m_console_idle = MIN( now - _sysapi_last_x_event, m_console_idle );
		}else {
			m_console_idle = now - _sysapi_last_x_event;
		}
	}

	if( m_console_idle != -1 ) {
		m_idle = MIN(m_console_idle, m_idle);
	}

	if( IsDebugVerbose( D_IDLE ) ) {
		dprintf( D_IDLE, "Idle Time: user= %lld , console= %lld seconds\n",
				 (long long)m_idle, (long long)m_console_idle );
	}
	return;
}

#if !defined(CONDOR_UTMPX)
#include <utmp.h>
#define UTMP_KIND utmp
#endif

#if defined(LINUX)
static const char *UtmpName = "/var/run/utmp";
static const char *AltUtmpName = "/var/adm/utmp";
// FreeBSD 9 made a clean break from utmp to utmpx
#elif defined(CONDOR_FREEBSD) && !defined(CONDOR_UTMPX)
static char *UtmpName = "/var/run/utmp";
static char *AltUtmpName = "";
#elif defined(CONDOR_FREEBSD) && defined(CONDOR_UTMPX)
#include <utmpx.h>
#define ut_name ut_user
static char *UtmpName = "/var/run/utx.active";
static char *AltUtmpName = "";
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

	/* freading structures directly out of a file is a pretty bad idea
		exemplified by the fact that the elements in the utmp
		structure may change size based upon if you are
		compiling in 32 bit or 64 bit mode, but the byte lengths
		of the fields in the utmp file might not. Maybe we should
		figure out the sactioned ways on each platform to do
		this if one exists. -psilord 01/09/2002 */

	FILE *fp;
	struct UTMP_KIND utmp_info;

	static bool warned_missing_utmp = false;

	if ((fp=safe_fopen_wrapper_follow(UtmpName,"r")) == NULL) {
		if ((fp=safe_fopen_wrapper_follow(AltUtmpName,"r")) == NULL) {
			if (!warned_missing_utmp) {
				dprintf(D_ALWAYS, "Utmp files %s and %s missing, assuming infinite keyboard idle time\n", UtmpName, AltUtmpName);
				warned_missing_utmp = true;
			}
			return answer; // is INT_MAX
		}
	}

		// fread returns number of items read, not bytes
	while (fread((char *)&utmp_info, sizeof(struct UTMP_KIND), 1, fp) == 1) {
		utmp_info.ut_line[UT_LINESIZE - 1] = '\0';
#if defined(LINUX)
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

// Unix
time_t
all_pty_idle_time( time_t now )
{
	const char	*f;

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
		dev = new Directory( "/dev" );
	}

	for( dev->Rewind();  (f = dev->Next()); ) {
		if( strncmp("tty",f,3) == MATCH || strncmp("pty",f,3) == MATCH ) 
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
		for( dev_pts->Rewind();  (f = dev_pts->Next()); ) {
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
#if defined(LINUX) 
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
#elif defined(Darwin) || defined(CONDOR_FREEBSD)
#include <sys/types.h>
#else
#include <sys/mkdev.h>
#endif

// Unix
time_t
dev_idle_time( const char *path, time_t now )
{
	struct stat	buf;
	time_t answer;
	char pathname[100] = "/dev/";
	static int null_major_device = -1;

	if ( !path || path[0]=='\0' ||
		 strncmp(path,"unix:",5) == 0 ) {
		// we don't have a valid path, or it is
		// a nonuseful/fake path setup by the X server
		return now;
	}

	strncat( pathname, path, sizeof(pathname)-6 );
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
			dprintf( D_FULLDEBUG, "Error on stat(%s,%p), errno = %d(%s)\n",
					 pathname, &buf, errno, strerror(errno) );
		}
		buf.st_atime = 0;
	}

	/* XXX The signedness problem in this comparison is hard to fix properly */
	/*
		The first argument is there in case buf is uninitialized here.
		In this case, buf.st_atime would already be set to 0 above.
	*/
	if ( buf.st_atime != 0 && null_major_device > -1 &&
							null_major_device == int(major(buf.st_rdev))) {
		// this device is related to /dev/null, it should not count
		buf.st_atime = 0;
	}

	answer = now - buf.st_atime;
	if( buf.st_atime > now ) {
		answer = 0;
	}

	if( IsDebugVerbose( D_IDLE ) ) {
        dprintf( D_IDLE, "%s: %lld secs\n", pathname, (long long)answer );
	}

	return answer;
}

#if defined(LINUX)

#endif /* defined(LINUX) */

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

    mach_port_t             masterPort           = 0;
    io_iterator_t           hidObjectIterator    = 0;
    CFMutableDictionaryRef  hidMatchDictionary   = NULL;
    io_object_t             hidDevice            = 0;
    
    *user_idle = *console_idle = -1;
    
    if (IOMainPort(bootstrap_port, &masterPort) != kIOReturnSuccess) {
        dprintf(D_ALWAYS, "IDLE: Couldn't create a master I/O Kit port.\n");
    } else {
        hidMatchDictionary = IOServiceMatching("IOHIDSystem");
        if (IOServiceGetMatchingServices(masterPort, hidMatchDictionary, &hidObjectIterator) != kIOReturnSuccess) {
            dprintf(D_ALWAYS, "IDLE: Can't find IOHIDSystem\n");
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
	int64_t nanoseconds;
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
		// Convert from nanoseconds to seconds
		idle_time = nanoseconds / 1'000'000'000;
    }
	// CFRelease seems to be hip with taking a null object. This seems
	// strange to me, but hey, at least I thought about it
	CFRelease(object);
    return idle_time;
}

#endif  /* the end of the Mac OS X code, and the  */

void sysapi_idle_time_raw(time_t *m_idle, time_t *m_console_idle)
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

void sysapi_idle_time(time_t *m_idle, time_t *m_console_idle)
{
	sysapi_internal_reconfig();

	sysapi_idle_time_raw(m_idle, m_console_idle);
}
