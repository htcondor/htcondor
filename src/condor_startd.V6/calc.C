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
#include "startd.h"

int
calc_disk()
{
	return free_fs_blocks( exec_path );
}


#if defined(WIN32)

// ThreadInteract allows the calling thread to access the visable,
// interactive desktop in order to set a hook. 
BOOL ThreadInteract(HDESK & hdesk, HWINSTA & hwinsta)   
{      
	HDESK   hdeskTest;
	
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

	return TRUE;
}


extern "C" static DWORD WINAPI
message_loop_thread(void *)
{
	MSG msg;
	HDESK hdesk;
	HWINSTA hwinsta;

	// first allow this thread access to the interactive(visable) desktop
	while ( !ThreadInteract(hdesk,hwinsta) ) {
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
calc_idle_time( time_t & user_idle, time_t & console_idle)
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
		threadHandle = ::CreateThread(NULL, 0, message_loop_thread, 
									NULL, 0, &threadID);
		if (threadHandle == NULL) {
			dprintf(D_ALWAYS, "failed to create message loop thread, errno = %d\n",
					GetLastError());
			user_idle = console_idle = -1;
		}		
	} else {

		// Not the first time calc_idle_time has been called

		if ( KBQuery() ) {
			// a keypress was detected
			last_x_event = now;
		} else {
			// no keypress detected, test if mouse moved
			POINT current_pos;
			if ( ! ::GetCursorPos(&current_pos) ) {
				dprintf(D_ALWAYS,"GetCursorPos failed\n");
			} else {
				if ( (current_pos.x != previous_pos.x) || 
					(current_pos.y != previous_pos.y) ) {
					// the mouse has moved!
					previous_pos = current_pos;	// stash new position
					last_x_event = now;
				}
			}
		}

	}

	user_idle = now - last_x_event;
	console_idle = user_idle;

	dprintf( D_FULLDEBUG, "Idle Time: user= %d , console= %d seconds\n",
			 (int)user_idle, (int)console_idle );

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
calc_idle_time( time_t & user_idle, time_t & console_idle )
{
	time_t tty_idle;
	time_t now = time( 0 );
	char* tmp;

		// Find idle time from ptys/ttys.  See if we should trust
		// utmp.  If so, only stat the devices that utmp says are
		// associated with active logins.  If not, stat /dev/tty* and
		// /dev/pty* to make sure we get it right.
	tmp = param( "STARTD_HAS_BAD_UTMP" );
	if( tmp && (*tmp == 'T' || *tmp =='t') ) {
		user_idle = all_pty_idle_time( now );
	} else {
		user_idle = utmp_pty_idle_time( now );
	}
	if( tmp ) free( tmp );

		// Now, if CONSOLE_DEVICES is defined in the config file, stat
		// those devices and report console_idle.  If it's not there,
		// console_idle is -1.
	console_idle = -1;  // initialize
	if( console_devices ) {
		console_devices->rewind();
		while( (tmp = console_devices->next()) ) {
			tty_idle = dev_idle_time( tmp, now );
			user_idle = MIN( tty_idle, user_idle );
			if( console_idle == -1 ) {
				console_idle = tty_idle;
			} else {
				console_idle = MIN( tty_idle, console_idle );
			}
		}
	}

		// If we have the kbdd running, last_x_event will be set to
		// something useful, if not, it's just 0, in which case
		// user_idle will certainly be less than now.
	user_idle = MIN( now - last_x_event, user_idle );

		// If last_x_event != 0, then condor_kbdd told us someone did
		// something on the console, but always believe a /dev device
		// over the kbdd, so if console_idle is set, leave it alone.
	if( (console_idle == -1) && (last_x_event != 0) ) {
		console_idle = now - last_x_event;
	}

	dprintf( D_FULLDEBUG, "Idle Time: user= %d , console= %d seconds\n",
			 (int)user_idle, (int)console_idle );
	return;
}

#include <utmp.h>

#if defined(OSF1)
static char *UtmpName = "/var/adm/utmp";
static char *AltUtmpName = "/etc/utmp";
#elif defined(LINUX)
static char *UtmpName = "/var/run/utmp";
static char *AltUtmpName = "/var/adm/utmp";
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
	struct utmp utmp;

	if ((fp=fopen(UtmpName,"r")) == NULL) {
		if ((fp=fopen(AltUtmpName,"r")) == NULL) {
			EXCEPT("fopen of \"%s\"", UtmpName);
		}
	}

	while (fread((char *)&utmp, sizeof utmp, 1, fp)) {
#if defined(AIX31) || defined(AIX32) || defined(IRIX331) || defined(IRIX53) || defined(LINUX) || defined(OSF1)
		if (utmp.ut_type != USER_PROCESS)
#else
			if (utmp.ut_name[0] == '\0')
#endif
				continue;
		
		tty_idle = dev_idle_time(utmp.ut_line, now);
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
	static Directory *dev;
	time_t	idle_time;
	time_t	answer = (time_t)INT_MAX;

	if( !dev ) {
		dev = new Directory( "/dev" );
	}

	for( dev->Rewind();  (f = dev->Next()); ) {
		if( strncmp("tty",f,3) == MATCH || strncmp("pty",f,3) == MATCH ) {
			idle_time = dev_idle_time( f, now );
			if( idle_time < answer ) {
				answer = idle_time;
			}
		}
	}
	return answer;
}


time_t
dev_idle_time( char *path, time_t now )
{
	struct stat	buf;
	time_t answer;
	static char pathname[100] = "/dev/";

	strcpy( &pathname[5], path );
	if (stat(pathname,&buf) < 0) {
		dprintf( D_FULLDEBUG, "Error on stat(%s,0x%x), errno = %d\n",
				 pathname, &buf, errno );
		buf.st_atime = 0;
	}
	answer = now - buf.st_atime;
	if( answer < 0 ) {
		answer = 0;
	}
		// dprintf( D_FULLDEBUG, "%s: %d secs\n", pathname, (int)answer );
	return answer;
}

#endif /* defined(WIN32) */


#if defined(IRIX53)
#include <sys/sysmp.h>
#endif

#ifdef HPUX
#include <sys/pstat.h>
#endif

int
calc_ncpus()
{
#ifdef sequent
	int     cpus = 0;
	int     eng;

	if ((eng = tmp_ctl(TMP_NENG,0)) < 0) {
		perror( "tmp_ctl(TMP_NENG,0)" );
		exit(1);
	}

	while (eng--) {
		if( tmp_ctl(TMP_QUERY,eng) == TMP_ENG_ONLINE )
			cpus++;
	}
	return cpus;
#elif defined(HPUX)
        struct pst_dynamic d;
        if ( pstat_getdynamic ( &d, sizeof(d), (size_t)1, 0) != -1 ) {
          return d.psd_proc_cnt;
        }
        else {
          return 0;
        }
#elif defined(Solaris)
	return (int)sysconf(_SC_NPROCESSORS_ONLN);
#elif defined(IRIX53)
	return sysmp(MP_NPROCS);
#elif defined(WIN32)
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	return info.dwNumberOfProcessors;
#elif defined(LINUX)

	FILE        *proc;
	char 		buf[256];
	int 		num_cpus = 0;

	proc = fopen( "/proc/cpuinfo", "r" );
	if( !proc ) {
		return 1;
	}

/*
/proc/meminfo looks something like this:
(For 1 cpu machines, there's only 1 entry).

processor       : 0
cpu             : 686
model           : 3
vendor_id       : GenuineIntel
stepping        : 4
fdiv_bug        : no
hlt_bug         : no
f00f_bug        : no
fpu             : yes
fpu_exception   : yes
cpuid           : yes
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic 11 mtrr pge mca cmov mmx
bogomips        : 298.19

processor       : 1
cpu             : 686
model           : 3
vendor_id       : GenuineIntel
stepping        : 4
fdiv_bug        : no
hlt_bug         : no
f00f_bug        : no
fpu             : yes
fpu_exception   : yes
cpuid           : yes
wp              : yes
flags           : fpu vme de pse tsc msr pae mce cx8 apic 11 mtrr pge mca cmov mmx
bogomips        : 299.01
*/
	// Count how many lines begin with the string "processor".
	while( fgets( buf, 256, proc) ) {
		if( !strncmp( buf, "processor", 9 ) ) {
			num_cpus++;
		}
	}
	fclose( proc );
	return num_cpus;

#else sequent
	return 1;
#endif sequent
}


int
calc_mips()
{
	return dhry_mips();
}


int
calc_kflops()
{
	return clinpack_kflops();
}
