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
#include "procapi.h"
#include "condor_debug.h"

HashTable <pid_t, procHashNode *> * ProcAPI::procHash = 
    new HashTable <pid_t, procHashNode *> ( PHBUCKETS, hashFunc );  
#ifndef WIN32
piPTR ProcAPI::procFamily	= NULL;
piPTR ProcAPI::allProcInfos = NULL;
pidlistPTR ProcAPI::pidList	= NULL;
int ProcAPI::pagesize		= 0;
#ifdef LINUX
long unsigned ProcAPI::boottime	= 0;
long unsigned ProcAPI::boottime_expiration = 0;
#endif // LINUX
#else // WIN32
#include "ntsysinfo.h"
static CSysinfo ntSysInfo;	// for getting parent pid on NT
PPERF_DATA_BLOCK ProcAPI::pDataBlock	= NULL;
struct Offset * ProcAPI::offsets		= NULL;
#endif // WIN32

procHashNode::procHashNode()
{
	lasttime = 0.0;
	oldtime = 0.0;
	oldusage = 0.0;
	oldminf = 0L;
	oldmajf = 0L;
	majfaultrate = 0L;
	minfaultrate = 0L;
	creation_time = 0L;
	garbage = false;
}

ProcAPI::~ProcAPI() {
        // deallocate stuff like crazy.
#ifndef WIN32
    deallocPidList();
    deallocAllProcInfos();
    deallocProcFamily();
#endif

    struct procHashNode * phn;
    procHash->startIterations();
    while ( procHash->iterate( phn ) )
        delete phn;
    
    delete procHash;
    
#ifdef WIN32
    if ( offsets )
        free ( offsets );
    
    if ( pDataBlock )
        free ( pDataBlock );
#endif
}    

// getProcInfo is the heart of the procapi implementation.
// Yes, it's an absolute mess of #ifdef's....

int
ProcAPI::getProcInfo( pid_t pid, piPTR& pi ) 
{

// this version works for Solaris 2.5.1, IRIX, OSF/1 
#if ( defined(Solaris251) || defined(IRIX) || defined(OSF1) )

	char path[64];
	struct prpsinfo pri;
	struct prstatus prs;
#ifndef OSF1
	struct prusage pru;   // prusage doesn't exist in OSF/1
#endif

	int fd, rval = 0;
	priv_state priv = set_root_priv();

	initpi( pi );

		// if the page size has not yet been found, get it.
	if( pagesize == 0 ) {
		pagesize = getpagesize() / 1024;  // pagesize is in k now
	}  
	sprintf( path, "/proc/%d", pid );
	errno = 0;
	if( (fd = open(path, O_RDONLY)) >= 0 ) {
			// PIOCPSINFO gets memory sizes, pids, and age.
		rval = ioctl( fd, PIOCPSINFO, &pri );
		if( rval >= 0 ) {
			pi->imgsize = pri.pr_size * pagesize;
			pi->rssize  = pri.pr_rssize * pagesize;
			pi->pid     = pri.pr_pid;
			pi->ppid    = pri.pr_ppid;
			pi->age     = secsSinceEpoch() - pri.pr_start.tv_sec;
			pi->creation_time = pri.pr_start.tv_sec;
		} else {
			dprintf( D_ALWAYS, 
					 "ProcAPI: PIOCPSINFO Error occurred for pid %d\n",
					 pid );
			close( fd );
			delete pi; 
			pi = NULL;
			set_priv( priv );
			return -2;
		}
		long nowminf, nowmajf;

    // PIOCUSAGE is used for page fault info
    // solaris 2.5.1 and Irix only - unsupported by osf/1
#ifndef OSF1
		rval = ioctl( fd, PIOCUSAGE, &pru );
		if( rval >= 0 ) {

#ifdef Solaris251   
			nowminf = pru.pr_minf;  
			nowmajf = pru.pr_majf;  
#endif

#ifdef IRIX   // dang things named differently in irix.
			nowminf = pru.pu_minf;  // Irix:  pu_minf, pu_majf.
			nowmajf = pru.pu_majf;  
#endif

		} else {
			dprintf( D_ALWAYS, 
					 "ProcAPI: PIOCUSAGE Error occurred for pid %d\n", 
					 pid );
			close( fd );
			delete pi; 
			pi = NULL;
			set_priv( priv );
			return -2;
		}
	
#else  //here we are in osf/1, which doesn't give this info.
		nowminf = 0;   // let's default to zero in osf1
		nowmajf = 0;
#endif

    // PIOCSTATUS gets process user & sys times
    // this following bit works for Sol 2.5.1, Irix, Osf/1
		rval = ioctl( fd, PIOCSTATUS, &prs );
		if( rval >= 0 ) {
			pi->user_time   = prs.pr_utime.tv_sec;
			pi->sys_time    = prs.pr_stime.tv_sec;

 	/* The bastard os lies to us and can return a negative number for stime.
       ps returns the wrong number in its listing, so this is an IRIX bug
       that we cannot work around except this way */
#if defined(IRIX)
			if (pi->user_time < 0)
				pi->user_time = 0;

			if (pi->sys_time < 0)
				pi->sys_time = 0;
#endif

      /* here we've got to do some sampling ourself.  If the pid is not in
         the hashtable, put it there using (user+sys time) / age as %cpu.
         If it is there, use ((user+sys time) - old time) / timediff.
      */
			
			double ustime = ( prs.pr_utime.tv_sec + 
							  ( prs.pr_utime.tv_nsec * 1.0e-9 ) ) +
				            ( prs.pr_stime.tv_sec + 
							  ( prs.pr_stime.tv_nsec * 1.0e-9 ) );

			/* it is possible to get negative numbers in IRIX out of /proc */
#if defined(IRIX)
			if(ustime < 0)
				ustime = 0.0;
#endif
			do_usage_sampling( pi, ustime, nowmajf, nowminf );

		} else {
			dprintf( D_ALWAYS, 
					 "ProcAPI: PIOCSTATUS Error occurred for pid %d\n", 
					 pid );
			delete pi;
			pi = NULL;
			rval = -2;
		}
			// close the /proc/pid file
		close ( fd );
	} else {
		if( errno == ENOENT ) {
				// /proc/pid doesn't exist
			dprintf( D_FULLDEBUG, "ProcAPI: pid %d does not exist.\n", pid );
			rval = -1;
		} else if ( errno == EACCES ) {
			dprintf( D_FULLDEBUG, "ProcAPI: No permission to open %s.\n", 
					 path );
			rval = -2;
		} else {
			dprintf( D_ALWAYS, "ProcAPI: Error opening %s, errno: %d.\n", 
					 path, errno );
			rval = -2;
		}
		delete pi;
		pi = NULL;
	}
	set_priv( priv );
	return rval;

#endif /* defined(Solaris251) || defined(IRIX) || defined(OSF1) */

// This is the version of getProcInfo for Solaris 2.6 and 2.7 and 2.8
#if defined(Solaris26) || defined(Solaris27) || defined(Solaris28)

	char path[64];
	int fd, rval = 0;
	psinfo_t psinfo;
	pstatus_t pstatus;
	prusage_t prusage;
	priv_state priv = set_root_priv();
	initpi ( pi );

  // pids, memory usage, and age can be found in 'psinfo':
	sprintf( path, "/proc/%d/psinfo", pid );
	if( (fd = open(path, O_RDONLY)) >= 0 ) { /*solaris success! */
		if( read(fd, &psinfo, sizeof(psinfo_t)) != sizeof(psinfo_t) ) {
			dprintf( D_ALWAYS, "ProcAPI: Problems reading %s.\n", path );
			rval = -2;
		}
		close( fd );
	} else {
		if( errno == ENOENT ) {
				// pid doesn't exist
			dprintf( D_FULLDEBUG, "ProcAPI: pid %d does not exist.\n", pid );
			rval = -1;
		} else if ( errno == EACCES ) {
			dprintf( D_FULLDEBUG, "ProcAPI: No permission to open %s.\n", 
					 path );
			rval = -2;
		} else { 
			dprintf( D_ALWAYS, "ProcAPI: Error opening %s, errno: %d.\n", 
					 path, errno );
			rval = -2;
		}
	} 
	if( rval ) {
		delete pi;
		pi = NULL;
		set_priv( priv );
		return rval;
	}

	pi->imgsize	= psinfo.pr_size;    // already in k!
	pi->rssize	= psinfo.pr_rssize;  // already in k!
	pi->pid		= psinfo.pr_pid;
	pi->ppid	= psinfo.pr_ppid;
	pi->age		= secsSinceEpoch() - psinfo.pr_start.tv_sec;
	pi->creation_time = psinfo.pr_start.tv_sec;

  // maj/min page fault info and user/sys time is found in 'usage':
  // I have never seen minor page faults return anything 
  // other than '0' in 2.6.  I have seen a value returned for 
  // major faults, but not that often.  These values are suspicious.
	sprintf( path, "/proc/%d/usage", pid );
	if( (fd = open(path, O_RDONLY)) >= 0 ) { // solaris success!
		if( read(fd, &prusage, sizeof(prusage_t)) != sizeof(prusage_t) ) {
			dprintf( D_ALWAYS, "ProcAPI: Problems reading %s.\n", path );
			rval = -2;
		}
		close( fd );
	} else {
		if( errno == ENOENT ) {
				// pid doesn't exist
			dprintf( D_FULLDEBUG, "ProcAPI: pid %d does not exist.\n", pid );
			rval = -1;
		} else if ( errno == EACCES ) {
			dprintf( D_FULLDEBUG, "ProcAPI: No permission to open %s.\n", 
					 path );
			rval = -2;
		} else { 
			dprintf( D_ALWAYS, "ProcAPI: Error opening %s, errno: %d.\n", 
					 path, errno );
			rval = -2;
		}
	}
	if( rval ) {
		delete pi;
		pi = NULL;
		set_priv( priv );
		return rval;
	}

	long nowminf, nowmajf;

	nowminf = prusage.pr_minf;
	nowmajf = prusage.pr_majf;
	pi->user_time= prusage.pr_utime.tv_sec;
	pi->sys_time = prusage.pr_stime.tv_sec;

  /* Now we do that sampling hashtable thing to convert page faults
     into page faults per second */

	double ustime = ( prusage.pr_utime.tv_sec + 
					  (prusage.pr_utime.tv_nsec * 1.0e-9) ) +
		            ( prusage.pr_stime.tv_sec + 
					  (prusage.pr_stime.tv_nsec * 1.0e-9) ); 

	do_usage_sampling( pi, ustime, nowmajf, nowminf );

	set_priv( priv );
	return 0;

#endif /* Solaris26 || Solaris27 || Solaris28 */

// This is the Linux version of getProcInfo.  Everything is easier and
// actually seems to work in Linux...nice, but annoyingly different.
#ifdef LINUX

	char path[64];
	FILE *fp;

	long usert, syst, jiffie_start_time, start_time;
	unsigned long now;
	unsigned long vsize, rss;
	long nowminf, nowmajf;
	
	long i;
	int rval = 0;
	unsigned long u;
	char c;
	char s[256], junk[16];

	priv_state priv = set_root_priv();

	initpi( pi );

	sprintf( path, "/proc/%d/stat", pid );

	int number_of_attempts = 0;
	while (number_of_attempts < 5) {
		number_of_attempts++;
		if( (fp = fopen(path, "r")) != NULL ) {
			fscanf( fp, "%d %s %c %d "
				"%ld %ld %ld %ld "
				"%lu %lu %lu %lu %lu "
				"%ld %ld %ld %ld %ld %ld "
				"%lu %lu %ld %lu %lu %lu %lu %lu %lu %lu %lu "
				"%ld %ld %ld %ld %lu",
				&pi->pid, s, &c, &pi->ppid, 
				&i, &i, &i, &i, 
				&u, &nowminf, &u, &nowmajf, &u, 
				&usert, &syst, &i, &i, &i, &i, 
				&u, &u, &jiffie_start_time, &vsize, &rss, &u, &u, &u, 
				&u, &u, &u, &i, &i, &i, &i, &u );
			fclose( fp );
			// Perform sanity check on the data we just read, as
			// sometimes Linux screws it up.  
			if ( pid == pi->pid ) {
					// data looks ok.  set rval to success.
					rval = 0;
					// and break out of the loop.
					break;
			}
			// if we've made it here, pid != pi->pid, which tells
			// us that Linux screwed up and the info we just
			// read from /proc is screwed.  so set rval appropriately,
			// and we'll either try again or give up based on
			// number_of_attempts.
			rval = -3;
		} else {
			if( errno == ENOENT ) {
				// /proc/pid doesn't exist
				dprintf( D_FULLDEBUG, "ProcAPI: pid %d does not exist.\n", 
					 pid );
				rval = -1;
				break;
			} else if ( errno == EACCES ) {
				dprintf( D_FULLDEBUG, "ProcAPI: No permission to open %s.\n", 
					 path );
				rval = -2;
				break;
			} else { 
				dprintf( D_ALWAYS, "ProcAPI: Error opening %s, errno: %d.\n", 
					 path, errno );
				rval = -2;
				break;
			}
		}
	} 	// end of while number_of_attempts < 0

	if ( rval < 0 ) {
		if ( rval == -3 ) {
			dprintf(D_ALWAYS,
				"ProcAPI: ERROR: /proc has bad data for pid %d\n",pid);
			// change rval to be -2; callers expect to see just -1 or -2
			rval = -2;
		}
		delete pi;
		pi = NULL;
		set_priv( priv );
		return rval;
	}
		// if the page size has not yet been found, get it.
	if( pagesize == 0 ) {
		pagesize = getpagesize() / 1024;
	}

	pi->user_time   = usert   / HZ;			// convert jiffies to sec.
	pi->sys_time    = syst    / HZ;
	pi->imgsize		= (vsize / 1024);			// bytes to k.
	pi->rssize		= rss * pagesize;			// pages to k.
    
		// we have start_time, which is the time after system boot
		// that the process started...convert jiffies to seconds.
	start_time = jiffie_start_time / HZ;
  
	now = secsSinceEpoch();

		// get the system boot time periodically, since it may change
		// if the time on the machine is adjusted
	if( boottime_expiration <= now ) {
		if( (fp = fopen("/proc/stat", "r")) > 0 ) {
			fgets( s, 256, fp );
			while( strstr(s, "btime") == NULL ) {
				fgets( s, 256, fp );
			}
			sscanf( s, "%s %lu", junk, &boottime );
			fclose( fp );
			boottime_expiration = now+60; // update once every minute
		} else if (boottime == 0) {
				// we failed to get the boottime, so we must abort
				// unless we have an old boottime value to fall back on
			dprintf( D_ALWAYS, "ProcAPI: Problem opening /proc/stat "
					 "for btime.\n" );
			delete pi; 
			pi = NULL;
			set_priv( priv );
			return -2;
		}
	}
		// now we've got the boottime, the start time, and can get
		// current time.  Throw 'em all together:  (yucky)

	pi->age = (now - boottime) - start_time;

		// There seems to be something weird about jiffie_start_time
		// in that it can grow faster than the process itself.  This
		// can result in a negative age (on both SMP and non-SMP 2.0.X
		// kernels, at least).  So, instead of allowing a negative
		// age, which will lead to a negative cpu usage and
		// CondorLoadAvg, we just set it to 0, since that's basically
		// what's up.  -Derek Wright and Mike Yoder 3/24/99.
	if( pi->age < 0 ) {
		pi->age = 0;
	}

	pi->creation_time = now - pi->age;

  /* here we've got to do some sampling ourself to get the cpuusage
	 and make the page faults a rate...
  */

	double ustime = (usert + syst) / (double)HZ;

	do_usage_sampling ( pi, ustime, nowmajf, nowminf );

		// Note: sanity checking done in above call.

	set_priv( priv );
	return 0;

#endif /* Linux */

/* Here's getProcInfo for HPUX.  Calling this a /proc interface is a lie, 
   because there IS NO /PROC for the HPUX's.  I'm using pstat_getproc().
   It returns process-specific information...pretty good, actually.  When
   called with a 3rd arg of 0 and a 4th arg of a pid, the info regarding
   that pid is returned in buf.  The bonus is, everything works...and
   you can get info on every process.
*/
#ifdef HPUX

	struct pst_status buf;
	int rval = 0;
	long nowminf, nowmajf, oldminf, oldmajf;

	priv_state priv = set_root_priv();

	initpi( pi );

	rval = pstat_getproc( &buf, sizeof(buf), 0, pid );

	if ( rval < 0 ) {
		dprintf( D_ALWAYS, "ProcAPI: Error in pstat_getproc(%d): errno=%d\n",
				 pid, errno );
		delete pi; 
		pi = NULL;
		set_priv( priv );
		return -2;
	}

	if( pagesize == 0 ) {
		pagesize = getpagesize() / 1024;  // pagesize is in k now
	}

		// I have personally seen a case where the resident set size
		// was BIGGER than the image size.  However, 'top' agreed with
		// my measurements, so I guess it's just a goofy
		// bug/feature/whatever in HPUX.

	pi->imgsize	= (buf.pst_vdsize+buf.pst_vtsize+buf.pst_vssize) * pagesize;
	pi->rssize	= buf.pst_rssize * pagesize;

	nowminf		= buf.pst_minorfaults;
	nowmajf		= buf.pst_majorfaults;

	pi->user_time	= buf.pst_utime;
	pi->sys_time	= buf.pst_stime;
	pi->age			= secsSinceEpoch() - buf.pst_start;
	pi->creation_time = buf.pst_start;

	pi->pid		= buf.pst_pid;
	pi->ppid	= buf.pst_ppid;

		/* Now that darned sampling thing, so that page faults gets
		   converted to page faults per second */

		// This is very inaccurrate: the below are in SECONDS!
	double ustime = ((double) buf.pst_utime + buf.pst_stime );

	do_usage_sampling( pi, ustime, nowmajf, nowminf );

	set_priv( priv );
	return 0;

#endif /* HPUX */

#ifdef WIN32
/* Danger....WIN32 code follows....
   The getProcInfo call for WIN32 actually gets *all* the information
   on all the processes, but that's not preventable using only
   documented interfaces.  
   So, becase getProcInfo is so expensive on Win32, we cheat a little
   and try to determine if the pid is still
   around before we drudge through all this code. */

	// So to first see if this pid is still alive
	// on Win32, open a handle to the pid and call GetExitStatus
	int status = FALSE;
	HANDLE pidHandle = ::OpenProcess(PROCESS_QUERY_INFORMATION,FALSE,pid);
	if (pidHandle) {
		DWORD exitstatus;
		if ( ::GetExitCodeProcess(pidHandle,&exitstatus) ) {
			if ( exitstatus == STILL_ACTIVE )
				status = TRUE;
		}
		::CloseHandle(pidHandle);
	} else {
		// OpenProcess() may have failed
		// due to permissions, or because all handles to that pid are gone.
		if ( ::GetLastError() == 5 ) {
			// failure due to permissions.  this means the process object must
			// still exist, although we have no idea if the process itself still
			// does or not.  error on the safe side; return TRUE.
			status = TRUE;
		} else {
			// process object no longer exists, so process must be gone.
			status = FALSE;	
		}
	}
    if ( status == FALSE ) {
        dprintf( D_FULLDEBUG, "ProcAPI: pid # %d was not found\n", pid );
		return -1;
    }
	
	// pid appears to still be around, so get the stats on it

    DWORD dwStatus;  // return status of fn. calls

/* Note to whom it may concern:  we DO NOT need to set_root_priv here!
   NT makes this shit available to any old process.  I've left this
   in and commented out for demonstration purposes, and have removed
   the rest of the priv stuff from the NT code. */
//	priv_state priv = set_root_priv();

    initpi ( pi );

        // '2' is the 'system' , '230' is 'process'  
        // I hope these numbers don't change over time... 
    dwStatus = GetSystemPerfData ( TEXT("230") );

    if( dwStatus != ERROR_SUCCESS ) {
        dprintf( D_ALWAYS, "ProcAPI: getProcInfo() failed to get "
				 "performance info.\n");
        return -2;
    }

        // somehow we don't have the process data -> panic
    if( pDataBlock == NULL ) {
        dprintf( D_ALWAYS, "ProcAPI: getProcInfo() failed to make "
				 "pDataBlock.\n");
        return -2;
    }
    
    PPERF_OBJECT_TYPE pThisObject;
    PPERF_INSTANCE_DEFINITION pThisInstance;
    long instanceNum;
    DWORD ctrblk;

    double sampleObjectTime;
    double objectFrequency;

    pThisObject = firstObject (pDataBlock);
  
    sampleObjectTime = LI_to_double ( pThisObject->PerfTime );
    objectFrequency  = LI_to_double ( pThisObject->PerfFreq );

    if( !offsets ) {      // If we haven't yet gotten the offsets, grab 'em.
        grabOffsets( pThisObject );
	}    
        // at this point we're all set to march through the data block to find
        // the instance with the pid we want.  

    pThisInstance = firstInstance(pThisObject);
    instanceNum = 0;
    bool found = false;
    pid_t thispid;

    while( (!found) && (instanceNum < pThisObject->NumInstances) ) {
        ctrblk = ((DWORD)pThisInstance) + pThisInstance->ByteLength;
        thispid = (long) *((long*)(ctrblk + offsets->procid));
        if( thispid == pid ) {
            found = true;
        } else {
            instanceNum++;
            pThisInstance = nextInstance( pThisInstance );
        }
    }

    if( !found ) {
        dprintf( D_FULLDEBUG, "ProcAPI: pid # %d was not found\n", pid );
		return -1;
    }

    LARGE_INTEGER elt= (LARGE_INTEGER) 
        *((LARGE_INTEGER*)(ctrblk + offsets->elapsed));
    LARGE_INTEGER pt = (LARGE_INTEGER) 
        *((LARGE_INTEGER*)(ctrblk + offsets->pctcpu));
    LARGE_INTEGER ut = (LARGE_INTEGER) 
        *((LARGE_INTEGER*)(ctrblk + offsets->utime));
    LARGE_INTEGER st = (LARGE_INTEGER) 
        *((LARGE_INTEGER*)(ctrblk + offsets->stime));

    pi->pid       = (long) *((long*)(ctrblk + offsets->procid  ));
    pi->ppid      = ntSysInfo.GetParentPID(pi->pid);
    pi->imgsize   = (long) (*((long*)(ctrblk + offsets->imgsize ))) / 1024;
    pi->rssize    = (long) (*((long*)(ctrblk + offsets->rssize  ))) / 1024;
    pi->minfault  = 0;  // not supported by NT; all faults lumped into major.
    pi->user_time = (long) (LI_to_double( ut ) / objectFrequency);
    pi->sys_time  = (long) (LI_to_double( st ) / objectFrequency);
    pi->age       = (long) ((sampleObjectTime - LI_to_double ( elt )) 
                         / objectFrequency);
	pi->creation_time = (long) ((sampleObjectTime/objectFrequency) - 
								((sampleObjectTime - LI_to_double(elt)) /
								 objectFrequency) ); 

    double cpu = LI_to_double( pt ) / objectFrequency;
    long faults = (long) *((long*)(ctrblk + offsets->faults  ));

	do_usage_sampling ( pi, cpu, faults, 0, 
		sampleObjectTime/objectFrequency );

    return 0;

#endif  // WIN32 getProcInfo

} // 500+ lines later, the closing brace of getProcInfo! (Whew!)

// used for sampling the cpu usage and page faults over time.
void
ProcAPI::do_usage_sampling( piPTR& pi, 
						    double ustime, 
                            long nowmajf, 
							long nowminf
#ifdef WIN32  /* In WIN32, we must pass in the time now */
							, double now
#endif
							) {

/* About the pass-in-the-time if you're WIN32 hack:  Sorry.  It's just
   that we've got that info back in the functions that call this, 
   and I know for a fact that it's accurate and is the value we 
   want to use.  -MEY */

	double timediff;
	struct procHashNode * phn = NULL;

#ifndef WIN32  /* In Unix, we get the time here. (Ugh) -MEY */
	struct timeval thistime;
	double now;
	gettimeofday( &thistime, 0 );
	now = convertTimeval( thistime );
#endif

	/*	Do garbage collection on procHashNodes.  We throw out any
		procHashNode which we have not performed a lookup on in
		over an hour.  -Todd <tannenba@cs.wisc.edu> */
	static double last_garbage_collection_time = 0.0;	
	pid_t garbage_pid;
	if ( now - last_garbage_collection_time > 3600 ) {
		last_garbage_collection_time = now;
			// first delete anything still flagged as garbage
		procHash->startIterations();
		while ( procHash->iterate( garbage_pid, phn ) ) {
			if ( phn->garbage == true ) {
				// it is still flagged as garbage; delete it
				procHash->remove(garbage_pid);
				delete phn;
				phn = NULL;
			} else {
				// it is not still flagged as garbarge; so do 
				// not delete it, but instead reset the garbage
				// flag to true.  It will be cleared when/if we
				// perform a lookup on this node.
				phn->garbage = true;
			}
		}	// end of while loop to iterate through the hash table
	}	// end of if it is garbage collection time

		/* The creation_time has been added because we could store the
		   pid in the hashtable, the process would exit, we'd loop
		   through the pid space & a new process would take that pid,
		   and then we'd wind up using the old, dead pid's info as the
		   last record of the new pid.  That is wrong.  So, we use
		   each pid's creation time as a secondary identifier to make
		   sure we've got the right one. Mike & Derek 3/24/99 */

	phn = NULL;	// clear to NULL before attempting the lookup
	if (procHash->lookup( pi->pid, phn ) == 0) {

		/* Allow 2 seconds "slack" on creation time, like we do in
		   ProcFamily, since (at least on Linux) the value can
		   oscillate. Jim B. 11/29/00 */

		long birth = phn->creation_time - pi->creation_time;
		if (-2 > birth || birth > 2) {
			/* must be a different process associated with this, remove it. */
			procHash->remove(pi->pid);
			delete phn;
			phn = NULL;
		}
	}
	if( phn ) {
			// success; pid in hash table

			/*	clear the garbage flag.  we need to do this whenever
				we reference a procHashNode so it does not get deleted
				prematurely. */
		phn->garbage = false;
		
		timediff = now - phn->lasttime;
            /* do some sanity checking now.  The Solaris 2.6 kernel
               has lied to us about the user & sys time.  They will 
               have a value at one reading, then 5 seconds later
               they will be back to zero.  This results in a negative
               cpuusage and problems for us.  Check for this and 
               use old values if it happens.  -MEY 4-9-1999 */
        if ( ustime < phn->oldtime ) {
                // lying bastard OS! (use old vals)
            pi->cpuusage = phn->oldusage;
            pi->minfault = phn->oldminf;
            pi->majfault = phn->oldmajf;
        } else { 
                // OS not lying:
            if ( timediff < 1.0 ) {  // less than one second since last poll
                pi->cpuusage = phn->oldusage;
                pi->minfault = phn->minfaultrate;
                pi->majfault = phn->majfaultrate;
                now     = phn->lasttime;  // we want old values preserved...
                ustime  = phn->oldtime;
                nowminf = phn->oldminf;
                nowmajf = phn->oldmajf;
            } else {
                pi->cpuusage = ( ( ustime - phn->oldtime ) / timediff ) * 100;
                pi->minfault = (long unsigned)
                    ((nowminf - phn->oldminf)/timediff);
                pi->majfault = (long unsigned)
                    ((nowmajf - phn->oldmajf)/timediff);
            }
        } // end OS not lying case
    } else {
            // pid not in hash table; first time.  Use age of
            // process as guide 
        if( pi->age == 0 ) {
            pi->cpuusage = 0.0;
            pi->minfault = 0;
            pi->majfault = 0;
        } else {
            pi->cpuusage = ( ustime / (double) pi->age ) * 100;
            pi->minfault = (long unsigned)(nowminf / (double) pi->age);
            pi->majfault = (long unsigned)(nowmajf / (double) pi->age);
        }
    } 

		// if we got that phn from the hashtable, remove it now.
	if ( phn ) {
		procHash->remove(pi->pid);
	}

		// put new vals back into hashtable
	struct procHashNode * new_phn = new procHashNode;
	new_phn->lasttime = now;
	new_phn->oldtime  = ustime;   // store raw data for next call...
	new_phn->oldminf  = nowminf;  //  ""
	new_phn->oldmajf  = nowmajf;  //  ""
	new_phn->oldusage = pi->cpuusage;  // Also store results in case the
	new_phn->minfaultrate = pi->minfault;   // next sample is < 1 sec
	new_phn->majfaultrate = pi->majfault;   // from now.
	new_phn->creation_time = pi->creation_time;
	procHash->insert( pi->pid, new_phn );

		// due to some funky problems, do some sanity checking here for
		// strange numbers:

		// cpuusage is giving some strange numbers.  Some of this may be
		// a bug in the Linux SMP kernel, or there may be a problem 
		// in the above code.
	if( pi->cpuusage < 0.0 ) {
        dprintf ( D_ALWAYS, "ProcAPI sanity failure, cpuusage = %f\n", 
                  pi->cpuusage );
        pi->cpuusage = 0.0;
	}
	if( pi->user_time < 0 ) {
		dprintf ( D_ALWAYS, "ProcAPI sanity failure, user_time = %ld\n", 
				  pi->user_time );
		pi->user_time = 0;
	}
	if( pi->sys_time < 0 ) {
		dprintf ( D_ALWAYS, "ProcAPI sanity failure, sys_time = %ld\n", 
				  pi->sys_time );
		pi->sys_time = 0;
	}
	if( pi->age < 0 ) {
		dprintf ( D_ALWAYS, "ProcAPI sanity failure, age = %ld\n", 
				  pi->age );
		pi->age = 0;
	}

		// now we can delete this.
	if ( phn ) delete phn;
	
}

/* The next function, getMemInfo, is different for each *&^%$#@! OS.
   Each uses something quite different than other OS's use...
   A 0 is returned on success, and a -1 returned on failure.  The numbers
   returned for total & available mem are consistent with the numbers
   reported by top.  */

#if ( defined(Solaris)  )
int
ProcAPI::getMemInfo( int& totalmem, int& availmem ) {
	if( pagesize == 0 ) {
		pagesize = getpagesize() / 1024;
	}
	totalmem = sysconf(_SC_PHYS_PAGES) * pagesize;
	availmem = sysconf(_SC_AVPHYS_PAGES) * pagesize;
	return 0;
}
#endif /* Solaris */

#ifdef LINUX
int
ProcAPI::getMemInfo( int& totalmem, int& availmem ) {
  
	FILE *fd;
	char a[32];

	if( (fd = fopen("/proc/meminfo", "r")) > 0 ) {
		fscanf( fd, "%s %s %s %s %s %s"
				"%s %s %s %s %s %s %s"
				"%s %s %s %s"
				"%s %d %s"
				"%s %d %s", 
				a, a, a, a, a, a, 
				a, a, a, a, a, a, a,
				a, a, a, a,
				a, &totalmem, a, 
				a, &availmem, a );
		fclose( fd );
	} else {
		dprintf( D_ALWAYS, "ProcAPI: Error: Can't open /proc/meminfo.\n" );
		return -1;
	}
	return 0;
}
#endif /* LINUX */

#ifdef OSF1
int
ProcAPI::getMemInfo( int& totalmem, int& availmem ) {

	struct tbl_pmemstats s;
	if( table(TBL_PMEMSTATS, 0, (void *)&s, 1,sizeof(s) ) < 0 ) {
		return -1;
	}
	totalmem = s.physmem / 1024;
	if( pagesize == 0 ) {				// since we've got it right
		pagesize = s.pagesize / 1024;	// here, record pagesize if we
	}									// don't yet have it. 

	struct tbl_vmstats v;
	if( table(TBL_VMSTATS, 0, (void *)&v, 1, sizeof(v) ) < 0 ) {
		return -1;
	}    
	availmem = v.free_count * pagesize;
	return 0;
}
#endif /* OSF1 */

#ifdef IRIX
int
ProcAPI::getMemInfo( int& totalmem, int& availmem ) {
  
	struct rminfo rmi;

	if( pagesize == 0 ) {
		pagesize = getpagesize() / 1024;
	}
	if( sysmp(MP_SAGET, MPSA_RMINFO, &rmi, sizeof(rmi)) < 0 ) {
		dprintf( D_ALWAYS, "ProcAPI: Error in sysmp(), errno: %d\n",
				 errno );
		return -1;
	}
	totalmem = rmi.physmem * pagesize;
	availmem = rmi.freemem * pagesize;
	return 0;
}
#endif /* IRIX */

#ifdef HPUX
int
ProcAPI::getMemInfo( int& totalmem, int& availmem ) {

	struct pst_static s;
  
	if( pstat_getstatic(&s, sizeof(s), (size_t)1, 0) != -1 ) {
		pagesize = s.page_size / 1024;   // it's right here....
		totalmem = s.physical_memory * pagesize;
	} else {
		dprintf( D_ALWAYS, 
				 "ProcAPI: Error in pstat_getstatic(), errno: %d\n",
				 errno );
		return -1;
	}
	struct pst_dynamic d;
	if( pstat_getdynamic(&d, sizeof(d), (size_t)1, 0) != -1 ) {
		availmem = d.psd_free * pagesize;
	} else {
		dprintf( D_ALWAYS, 
				 "ProcAPI: Error in pstat_getdynamic(), errno: %d\n",
				 errno );
		return -1;
	}
	return 0;
}
#endif /* HPUX */

/* This version for windows is obviously broken. :-) */
#ifdef WIN32
int
ProcAPI::getMemInfo( int& totalmem, int& availmem ) {
    totalmem = 0;
    availmem = 0;
    return 0;
}
#endif /* WIN32 */

#ifndef WIN32
/* getProcSetInfo returns the sum of the procInfo structs for a set
   of pids.  These pids are specified by an array of pids ('pids') that
   has 'numpids' elements. */
int
ProcAPI::getProcSetInfo( pid_t *pids, int numpids, piPTR& pi ) {

	initpi ( pi );
	piPTR temp = NULL;
	int rval = 0, val = 0;
	priv_state priv;

	if( numpids <= 0 || pids == NULL ) {
			// We were passed nothing, so there's no work to do and we
			// should return immediately before we dereference
			// something we shouldn't be touching.
		return 0;
	}

	priv = set_root_priv();

	for( int i=0; i<numpids; i++ ) {
		val = getProcInfo( pids[i], temp );
		rval = MIN( rval, val );
		switch( val ) {
		case 0:
			pi->imgsize   += temp->imgsize;
			pi->rssize    += temp->rssize;
			pi->minfault  += temp->minfault;
			pi->majfault  += temp->majfault;
			pi->user_time += temp->user_time;
			pi->sys_time  += temp->sys_time;
			pi->cpuusage  += temp->cpuusage;
			if( temp->age > pi->age ) {
				pi->age = temp->age;
			}
			break;
		case -1:
			dprintf( D_FULLDEBUG, "ProcAPI::getProcSetInfo: Pid %d does "
					 "not exist, ignoring.\n", pids[i] );
			break;
		case -2:
			dprintf( D_ALWAYS, "ProcAPI::getProcSetInfo: Fatal error "
					 "getting info for pid %d.\n", pids[i] );
			break;
		default:
			dprintf( D_ALWAYS, "ProcAPI::getProcSetInfo: Unknown return "
					 "value (%d) from getProcInfo(%d)", val, pids[i] );
			rval = -2;
			break;
		}
	}

	if( temp ) { 
		delete temp;
	}
	set_priv( priv );
	return rval;
}
#endif

#ifdef WIN32
// There is a much better way to do this in WIN32...we get all instances
// of all processes at once anyway...
int
ProcAPI::getProcSetInfo( pid_t *pids, int numpids, piPTR& pi ) {

    initpi( pi );

	if( numpids <= 0 || pids == NULL ) {
			// We were passed nothing, so there's no work to do and we
			// should return immediately before we dereference
			// something we shouldn't be touching.
		return 0;
	}

	DWORD dwStatus;  // return status of fn. calls

        // '2' is the 'system' , '230' is 'process'  
        // I hope these numbers don't change over time... 
    dwStatus = GetSystemPerfData( TEXT("230") );
    
    if( dwStatus != ERROR_SUCCESS ) {
        dprintf( D_ALWAYS, "ProcAPI::getProcSetInfo failed to get "
				 "performance info.\n");
        return -1;
    }

        // somehow we don't have the process data -> panic
    if( pDataBlock == NULL ) {
        dprintf( D_ALWAYS, "ProcAPI::getProcSetInfo failed to make "
				 "pDataBlock.\n");
        return -1;
    }
    
    PPERF_OBJECT_TYPE pThisObject = firstObject (pDataBlock);

    if( !offsets ) {	// If we haven't yet gotten the offsets, grab 'em.
        grabOffsets( pThisObject );
    }

        // create local copy of pids.
	pid_t *localpids = (pid_t*) malloc ( sizeof (pid_t) * numpids );
	for( int i=0 ; i<numpids ; i++ ) {
		localpids[i] = pids[i];
	}
	
	multiInfo( localpids, numpids, pi );

	free(localpids);

	return 0;
}


int
ProcAPI::getFamilyInfo ( pid_t daddypid, piPTR& pi ) {

    initpi ( pi );

	if ( daddypid == 0 ) {
		return 0;
	}
	
	DWORD dwStatus;  // return status of fn. calls

		// '2' is the 'system' , '230' is 'process'  
        // I hope these numbers don't change over time... 
    dwStatus = GetSystemPerfData ( TEXT("230") );
    
    if ( dwStatus != ERROR_SUCCESS ) {
        dprintf( D_ALWAYS, "ProcAPI::getProcSetInfo failed to get "
				 "performance info.\n");
        return -1;
    }

        // somehow we don't have the process data -> panic
    if ( pDataBlock == NULL ) {
        dprintf( D_ALWAYS, "ProcAPI::getProcSetInfo failed to make "
				 "pDataBlock.\n");
        return -1;
    }
    
    PPERF_OBJECT_TYPE pThisObject = firstObject (pDataBlock);

    if ( !offsets )      // If we haven't yet gotten the offsets, grab 'em.
        grabOffsets( pThisObject );
    
	pid_t *allpids;
	pid_t *familypids;
	int numpids, familysize;

	// allpids gets allocated and filled in with pids.  numpids also returned.
	getAllPids( allpids, numpids );

	// returns the familypids resulting from the daddypid
	makeFamily( daddypid, allpids, numpids, familypids, familysize );

	multiInfo( familypids, familysize, pi );

	delete [] allpids;
	delete [] familypids;

	return 0;
}

int
ProcAPI::getPidFamily( pid_t daddypid, pid_t *pidFamily ) {

	if ( daddypid == 0 ) {
		pidFamily[0] = 0;
		return 0;
	}

	DWORD dwStatus;  // return status of fn. calls

	if ( pidFamily == NULL ) {
		dprintf( D_ALWAYS, 
				 "ProcAPI::getPidFamily: no space allocated for pidFamily\n" );
		return -1;
	}

        // '2' is the 'system' , '230' is 'process'  
        // I hope these numbers don't change over time... 
    dwStatus = GetSystemPerfData ( TEXT("230") );
    
	if ( dwStatus != ERROR_SUCCESS ) {
        dprintf( D_ALWAYS, 
				 "ProcAPI::getProcSetInfo failed to get performance info.\n");
        return -1;
    }

        // somehow we don't have the process data -> panic
    if ( pDataBlock == NULL ) {
        dprintf( D_ALWAYS, 
				 "ProcAPI::getProcSetInfo failed to make pDataBlock.\n");
        return -1;
    }
    
    PPERF_OBJECT_TYPE pThisObject = firstObject (pDataBlock);

    if ( !offsets )      // If we haven't yet gotten the offsets, grab 'em.
        grabOffsets( pThisObject );
    
	pid_t *allpids;
	pid_t *familypids;
	int numpids, familysize;

	// allpids gets allocated and filled in with pids.  numpids also returned.
	getAllPids( allpids, numpids );

	// returns the familypids resulting from the daddypid
	makeFamily( daddypid, allpids, numpids, familypids, familysize );
	
	for ( int q=0 ; q<familysize ; q++ ) {
		pidFamily[q] = familypids[q];
	}

	pidFamily[familysize] = 0;

	delete [] allpids;
	delete [] familypids;

	return 0;

}

void
ProcAPI::makeFamily( pid_t dadpid, pid_t *allpids, int numpids, 
					 pid_t* &fampids, int &famsize ) {

	pid_t *parentpids = new pid_t[numpids];
	fampids = new pid_t[numpids];  // just might include everyone...

	for( int i=0 ; i<numpids ; i++ ) {
		parentpids[i] = ntSysInfo.GetParentPID( allpids[i] );
	}

	famsize = 1;
	fampids[0] = dadpid;
	int numadditions = 1;

	while( numadditions > 0 ) {
		numadditions = 0;
		for( int j=0; j<numpids; j++ ) {
			if( isinfamily(fampids, famsize, parentpids[j]) ) {
				fampids[famsize] = allpids[j];
				parentpids[j] = 0;
				allpids[j] = 0;
				famsize++;
				numadditions++;
			}
		}
	}
	delete [] parentpids;
}

// allocate space for pids and fill in.
void
ProcAPI::getAllPids( pid_t* &pids, int &numpids ) {

    PPERF_OBJECT_TYPE pThisObject = firstObject (pDataBlock);
    PPERF_INSTANCE_DEFINITION pThisInstance = firstInstance(pThisObject);
	numpids = pThisObject->NumInstances;
	pids = new pid_t[numpids];
	DWORD ctrblk;
		
	for( int i=0 ; i<numpids ; i++ ) {
        ctrblk = ((DWORD)pThisInstance) + pThisInstance->ByteLength;
        pids[i] = (pid_t) *((pid_t*)(ctrblk + offsets->procid));
        pThisInstance = nextInstance(pThisInstance);        
	}
}


// We assume that the pDataBlock and offsets are all set up, and we have a
// set of pids (pidslist) to get info on.  Totals returned in pi.
int
ProcAPI::multiInfo( pid_t *pidlist, int numpids, piPTR &pi ) {

    PPERF_OBJECT_TYPE pThisObject = firstObject (pDataBlock);
    PPERF_INSTANCE_DEFINITION pThisInstance = firstInstance(pThisObject);

    double sampleObjectTime = LI_to_double ( pThisObject->PerfTime );
    double objectFrequency  = LI_to_double ( pThisObject->PerfFreq );

		// at this point we're all set to march through the data block to find
        // the pids that we want
    DWORD ctrblk;
	int instanceNum = 0;
    pid_t thispid;
    
	pi->ppid = -1;  // not possible for a set...
	pi->minfault  = 0;  // not supported by NT; all faults lumped into major.
	long maxage = 0;
	long total_faults = 0, faults = 0;
	double total_cpu = 0.0, cpu = 0.0;

	// loop through each instance in data, checking to see if on list

    while( instanceNum < pThisObject->NumInstances ) {
        
        ctrblk = ((DWORD)pThisInstance) + pThisInstance->ByteLength;
        thispid = (pid_t) *((pid_t*)(ctrblk + offsets->procid));

        if( isinlist(thispid, pidlist, numpids) != -1 ) {

            LARGE_INTEGER elt= (LARGE_INTEGER) 
                *((LARGE_INTEGER*)(ctrblk + offsets->elapsed));
			LARGE_INTEGER pt = (LARGE_INTEGER) 
                *((LARGE_INTEGER*)(ctrblk + offsets->pctcpu));
			LARGE_INTEGER ut = (LARGE_INTEGER) 
                *((LARGE_INTEGER*)(ctrblk + offsets->utime));
			LARGE_INTEGER st = (LARGE_INTEGER) 
                *((LARGE_INTEGER*)(ctrblk + offsets->stime));

			pi->pid      =  thispid;
			pi->imgsize  += (long) (*((long*)(ctrblk + offsets->imgsize ))) 
				/ 1024;
			pi->rssize   += (long) (*((long*)(ctrblk + offsets->rssize  ))) 
				/ 1024;
			pi->user_time+= (long) (LI_to_double( ut ) / objectFrequency);
			pi->sys_time += (long) (LI_to_double( st ) / objectFrequency);
			/* we put the actual ag in here for do_usage_sampling
			   purposes, and then set it to the max at the end */
			pi->age = (long) ((sampleObjectTime - LI_to_double ( elt )) 
                              / objectFrequency);
			if ( pi->age > maxage ) {
				maxage = pi->age;
			}
				/* Creation time of this process, used in identification. */
			pi->creation_time = (long) ((sampleObjectTime/objectFrequency) - 
									((sampleObjectTime - LI_to_double(elt)) /
									 objectFrequency) ); 

			/* We figure out the cpu usage (a total counter, not a 
			   percent!) and the total page faults here. */
            cpu = LI_to_double( pt ) / objectFrequency;
			faults = (long) *((long*)(ctrblk + offsets->faults  ));

			/* for this pid, figure out the %cpu and %faults */
			do_usage_sampling ( pi, cpu, faults, 0, 
				sampleObjectTime/objectFrequency );

			/* stuff these percentages back into a running total */
			total_cpu += pi->cpuusage;
			total_faults += pi->majfault;
			/* ready for use by next pid... */
			pi->cpuusage = 0.0;
			pi->majfault = 0;
		}

    // go to the next one...
		instanceNum++;
        pThisInstance = nextInstance( pThisInstance );        
    }    

	pi->pid = 0;    // It's the sum of many pids...
	pi->age = maxage;  // age is simply the max of the group.  
	pi->creation_time = (long) ((sampleObjectTime/objectFrequency) - maxage );
	pi->cpuusage = total_cpu;   // put our totals in here.
	pi->majfault = total_faults;  // ditto.
    return 0;
}

#endif // WIN32

#ifndef WIN32

/* This function returns a list of pids that are 'descendents' of that pid.
     I call this a 'family' of pids.  This list is put into pidFamily, which
     I assume is an already-allocated array.  This array will be terminated
     with a 0 for a pid at its end.  A -1 is returned on failure, 0 otherwise.
*/
int
ProcAPI::getPidFamily( pid_t pid, pid_t *pidFamily ) {
  // I'm going to do this in a somewhat ugly hacked way....get all the info
  // instead of just pids...but it's a lot quicker to write.

	if( pidFamily == NULL ) {
		dprintf( D_ALWAYS, 
				 "ProcAPI::getPidFamily: no space allocated for pidFamily\n" );
		return -1;
	}

#ifndef HPUX
	buildPidList();
#endif

	buildProcInfoList();

	if( buildFamily(pid) < 0 ) {  // can't get info on elder pid
		deallocPidList();
		deallocAllProcInfos();
		deallocProcFamily();
		return -1;
	}

	piPTR current = procFamily;  // get descendents and elder pid.
	int i=0;
		// we'll only return the first 511.  No self-respecting job
		// should have  more.  :-)  
	while( (current != NULL) && (i<511) ) {
		pidFamily[i] = current->pid;
		i++;
		current = current->next;
	}
  
	pidFamily[i] = 0;

		// deallocate all the lists of stuff...don't leave stale info
		// lying around. 
	deallocPidList();
	deallocAllProcInfos();
	deallocProcFamily();

	return 0;
}

/* This is the "given a pid, return information on that pid + all of its
   children" function.  Actually, this returns information on all of its
   _decendants_, in case the target pid forks a child that forks a child....

   Every OS EXCEPT for HPUX uses the following method:

   This function uses getProcInfo() so all the OS-specific stuff is hidden
   there, not here.  First, buildpidlist() gets all the pids in the system.
   Then, the linked list 'allProcInfos' is built using that information.

   Lastly, the 'procFamily' is built by scanning 'allProcInfos' repeatedly.
*/
int
ProcAPI::getFamilyInfo( pid_t daddypid, piPTR& pi ) {

#ifndef HPUX        // everyone except HPUX needs a pidlist built.
	buildPidList();
#endif

	buildProcInfoList();  // HPUX has its own version of this, too.

	if( buildFamily(daddypid) < 0 ) {  // return a -1 if we can't get info on
		deallocPidList();
		deallocAllProcInfos();
		deallocProcFamily();
		return -1;                        // daddypid.
	}

		// now, procFamily points to a list of the procInfos in that
		// family. 
	initpi( pi );

	pi->pid      = procFamily->pid;   // overall pid is this pid.
	pi->ppid     = procFamily->ppid;  // overall parent is parent's parent
	pi->age      = procFamily->age;   // let age simply be the age of the elder

	piPTR current = procFamily;

	while( current != NULL ) {

		pi->imgsize   += current->imgsize;
		pi->rssize    += current->rssize;
		pi->minfault  += current->minfault;
		pi->majfault  += current->majfault;
		pi->user_time += current->user_time;
		pi->sys_time  += current->sys_time;
		pi->cpuusage  += current->cpuusage;
		current = current->next;
	}

	pi->next = NULL;

		// deallocate all the lists of stuff...don't leave stale info
		// lying around. 
	deallocPidList();
	deallocAllProcInfos();
	deallocProcFamily();

	return 0;
}
#endif // not defined WIN32

/* initpi is a simple function that sets everything in a procInfo
   structure to a default value.  */
void
ProcAPI::initpi ( piPTR& pi ) {
	if( pi == NULL ) {
		pi = new procInfo;
	}
	pi->imgsize  = 0;
	pi->rssize   = 0;
	pi->minfault = 0;
	pi->majfault = 0;
	pi->user_time= 0;
	pi->sys_time = 0;
	pi->age      = 0;
	pi->cpuusage = 0.0;
	pi->pid      = -1;
	pi->ppid     = -1;
	pi->next     = NULL;
}

#ifndef WIN32  // Doesn't come close to working in WIN32...sigh.

/* This function returns the next pid in the pidlist.  That pid is then 
   removed from the pidList.  A -1 is returned when there are no more pids.
 */
pid_t
ProcAPI::getAndRemNextPid () {

	pidlistPTR temp;
	pid_t tpid;

	if( pidList == NULL ) {
		return -1;
	}
	temp = pidList;
	tpid = pidList->pid;
	pidList = pidList->next;
	delete temp;

	return tpid;
}

/* Wonderfully enough, this works for all OS's.  This function opens
   up the /proc directory and reads the pids of all the processes in the
   system.  This information is put into a list of pids that is pointed
   to by pidList, a private data member of ProcAPI.  
 */
int
ProcAPI::buildPidList() {

	DIR *dirp;
	struct dirent *direntp;
	pidlistPTR current;
	pidlistPTR temp;
	priv_state priv = set_root_priv();

		// make a header node for the pidList:
	deallocPidList();
	pidList = new pidlist;

	current = pidList;

	if( (dirp = opendir("/proc")) != NULL ) {
		while( (direntp = readdir(dirp)) != NULL ) {
			if( isdigit(direntp->d_name[0]) ) {   // check for first char digit
				temp = new pidlist;
				temp->pid = (pid_t) atol ( direntp->d_name );
				temp->next = NULL;
				current->next = temp;
				current = temp;
			}
		}
		closedir( dirp );
    
		temp = pidList;
		pidList = pidList->next;
		delete temp;           // remove header node.

		set_priv( priv );
		return 0;
	} else {
		delete pidList;        // remove header node.
		pidList = NULL;
		set_priv( priv );
		return -1;
	}
}

/* Using the list of processes pointed to by pidList and returned by
   getAndRemNextPid(), a linked list of procInfo structures is
   built.  At the end, allProcInfos will point to the head of this
   list.  This function is used on all OS's except for HPUX.
*/
#ifndef HPUX
int
ProcAPI::buildProcInfoList() {
  
	piPTR current;
	piPTR temp;
	pid_t thispid;

		// make a header node for ease of list construction:
	deallocAllProcInfos();
	allProcInfos = new procInfo;
	current = allProcInfos;
	current->next = NULL;

	temp = NULL;
	while( (thispid = getAndRemNextPid()) >= 0 ) {
		if( getProcInfo(thispid, temp) >= 0 ) {
			current->next = temp;
			current = temp;
			temp = NULL;
		}
	}

		// we're done; remove header node.
	temp = allProcInfos;
	allProcInfos = allProcInfos->next;
	delete temp;
	return 0;
}
#else
/* now for the HPUX version.  In a sense, it's simpler, because no
   pidlist has to be built......the allProcInfos list
   is built by repeated calls to pstat_getproc(), which will return
   information about every process in the system, if asked enough
   times and in the proper manner.
*/
int
ProcAPI::buildProcInfoList() {
  
		// this determines the number of process infos to grab at
		// once.   10 seems to be a good number.
	const int BURSTSIZE = 10;    

	piPTR current;
	piPTR temp;
	pid_t thispid;

	struct pst_status pst[BURSTSIZE];
	int i, count;
	int idx = 0;  // index within the context

		// make a header node for ease of list construction:
	deallocAllProcInfos();
	allProcInfos = new procInfo;
	current = allProcInfos;
	current->next = NULL;

	if( pagesize == 0 ) {
		pagesize = getpagesize() / 1024;  // pagesize is in k now
	}

		// loop until count == 0, which will occur when all have been returned
	while( (count = pstat_getproc(pst, sizeof(pst[0]), BURSTSIZE, idx)) > 0 ) {
		for( i=0 ; i<count; i++ ) {
			temp = new procInfo;

			temp->imgsize   = ( pst[i].pst_vdsize + pst[i].pst_vtsize 
								+ pst[i].pst_vssize )   * pagesize;
			temp->rssize    = pst[i].pst_rssize * pagesize;
			temp->minfault  = pst[i].pst_minorfaults;
			temp->majfault  = pst[i].pst_majorfaults;
			temp->user_time = pst[i].pst_utime;
			temp->sys_time  = pst[i].pst_stime;
			temp->age       = secsSinceEpoch() - pst[i].pst_start;
			temp->cpuusage  = pst[i].pst_pctcpu * 100;
			temp->pid       = pst[i].pst_pid;
			temp->ppid      = pst[i].pst_ppid;
			temp->next      = NULL;
			
			current->next = temp;
			current = temp;
		}
		idx = pst[count-1].pst_idx + 1;
	}

  // remove that header node:
	temp = allProcInfos;
	allProcInfos = allProcInfos->next;
	delete temp;

	if( count == -1 ) {
		dprintf( D_ALWAYS, "ProcAPI: pstat_getproc() failed\n" );
		return -1;
	}
	return 0;
}
#endif   // end of HPUX's buildProcInfoList()

/* buildFamily takes a list of procInfo structs pointed to by 
   allProcInfos and an ancestor pid 'daddypid' and builds a list
   of all procInfo structs and points 'procFamily' to it.
*/
int
ProcAPI::buildFamily( pid_t daddypid ) {

		// array of pids in family.  Size # pids.
	pid_t *familypids;
	int familysize = 0;
	int numprocs;

	if( (DebugFlags & D_FULLDEBUG) && (DebugFlags & D_PROCFAMILY) ) {
		dprintf( D_FULLDEBUG, 
				 "ProcAPI::buildFamily called w/ parent: %d\n", daddypid );
	}

	numprocs = getNumProcs();
	deallocProcFamily();
	procFamily = NULL;

		// make an array of size # processes for quick lookup of pids in family
	familypids = new pid_t[numprocs];

		// get the daddypid's procInfo struct
	piPTR pred, current, familyend;
	current = allProcInfos;

	while( (current != NULL) && (current->pid != daddypid) ) {
		pred = current;
		current = current->next;
	}

		// el problemo : if daddypid not in list at all, return -1
	if( current == NULL ) {
		delete [] familypids;
		dprintf( D_ALWAYS, "ProcAPI::buildFamily failed: parent %d "
				 "not found on system.\n", daddypid );
		return -1;
	}

		// special case: daddys is first on list:
	if( allProcInfos == current ) {
		procFamily = allProcInfos;
		allProcInfos = allProcInfos->next;
		procFamily->next = NULL;
	} else {  // regular case: daddy somewhere in middle
		procFamily = current;
		pred->next = current->next;
		procFamily->next = NULL;
	}

	familypids[0] = daddypid;
	familyend = procFamily;
	familysize = 1;

		// now, procFamily points at the procInfo struct for the
		// ancestral pid ( daddypid ).  Its pid is the 0th element in
		// familypids, and familyend will always point to the end of
		// the procFamily list.

	int numadditions = 1;
  
	while( numadditions != 0 ) {
		numadditions = 0;
		current = allProcInfos;
		while( current != NULL ) {
			if( isinfamily(familypids, familysize, current->ppid) ) {
				familypids[familysize] = current->pid;
				familysize++;

				familyend->next = current;

				if( current != allProcInfos ) {
					current = current->next;
					pred->next = current;
				} else {
					current = allProcInfos = allProcInfos->next;          
				}

				familyend = familyend->next;
				familyend->next = NULL;
				
				numadditions++;

			} else {
				pred = current;
				current = current->next;    
			}
		}
	}
	delete [] familypids;
	return 0;
}

/* This function returns the nuber of processes that allProcInfos 
   points at.  It is assumed that allProcInfos points to all the 
   processes in the system ( that buildProcInfoList has been called )
*/
int
ProcAPI::getNumProcs() {
	int nump;
	piPTR temp;
	temp = allProcInfos;
	for( nump=0; temp != NULL; nump++ ) {
		temp = temp->next;
	}
	return nump;
}


/* sec_since_epoch() gets the number of seconds since the 
   epoch.  Used to find the age of a process.  It uses the 
   time() function.  (Which is suprisingly supported by everyone!)
   It was simpler than I thought it would be. :-) */
long
ProcAPI::secsSinceEpoch() {
	return (long)time(NULL);
}

/* given a struct timeval, return that same value as a double */
double
ProcAPI::convertTimeval ( struct timeval t ) {
	return (double) t.tv_sec + ( t.tv_usec * 1.0e-6 );
}
#endif // not defined WIN32...sigh.

// Hey, here are two functions that can be used by both win32 and unix....
int
ProcAPI::isinfamily( pid_t *fam, int size, pid_t target ) {
	for( int i=0; i<size; i++ ) {
		if( fam[i] == target ) {
			return true;
		}
	}
	return false;
}

void
ProcAPI::printProcInfo( piPTR pi ) {

	if( pi == NULL ) {
		return;
	}
	printf( "process image, rss, in k: %lu, %lu\n", pi->imgsize, pi->rssize );
	printf( "minor & major page faults: %lu, %lu\n", pi->minfault, 
			pi->majfault ); 
	printf( "Times:  user, system, age: %ld %ld %ld\n", 
			pi->user_time, pi->sys_time, pi->age );
	printf( "percent cpu usage of this process: %5.2f\n", pi->cpuusage);
	printf( "\n" );
}

#ifndef WIN32
 
void
ProcAPI::deallocPidList() {
	if( pidList != NULL ) {
		pidlistPTR prev;
		pidlistPTR temp = pidList;
		while( temp != NULL ) {
			prev = temp;
			temp = temp->next;
			delete prev;
		}
		pidList = NULL;
	}
}

void 
ProcAPI::deallocAllProcInfos() {
	if( allProcInfos != NULL ) {
		piPTR prev;
		piPTR temp = allProcInfos;
		while( temp != NULL ) {
			prev = temp;
			temp = temp->next;
			delete prev;
		}
		allProcInfos = NULL;
	}
}

void
ProcAPI::deallocProcFamily() {
	if( procFamily != NULL ) {
		piPTR prev;
		piPTR temp = procFamily;
		while( temp != NULL ) {
			prev = temp;
			temp = temp->next;
			delete prev;
		}
		procFamily = NULL;
	}
}

#endif // not defined WIN32

int
hashFunc( const pid_t& pid, int numbuckets ) {
	return pid % numbuckets;   
}

// Warning: WIN32 stuff below.  Not for the faint of heart.
#ifdef WIN32

/* pointer functions, used to walk down various structures in 
   the perf data block returned by the call to RegQueryValueEx
*/

PPERF_OBJECT_TYPE ProcAPI::
firstObject( PPERF_DATA_BLOCK pPerfData ) {
	return( (PPERF_OBJECT_TYPE) ((PBYTE)pPerfData + 
								 pPerfData->HeaderLength) );
}

PPERF_OBJECT_TYPE ProcAPI::
nextObject( PPERF_OBJECT_TYPE pObject ) {
	return( (PPERF_OBJECT_TYPE) ((PBYTE)pObject +
								 pObject->TotalByteLength) );
}

PERF_COUNTER_DEFINITION * ProcAPI::
firstCounter ( PERF_OBJECT_TYPE *pObjectDef ) {
    return ( PERF_COUNTER_DEFINITION * ) ((PCHAR) pObjectDef + 
                                          pObjectDef->HeaderLength);
}

PERF_COUNTER_DEFINITION * ProcAPI::
nextCounter ( PERF_COUNTER_DEFINITION *pCounterDef ) {
    return ( PERF_COUNTER_DEFINITION * ) ((PCHAR) pCounterDef + 
                                          pCounterDef->ByteLength);
}

PERF_INSTANCE_DEFINITION * ProcAPI::
firstInstance ( PERF_OBJECT_TYPE *pObject ) {
    return ( PERF_INSTANCE_DEFINITION * ) ((PBYTE) pObject + 
                                           pObject->DefinitionLength);
}

PERF_INSTANCE_DEFINITION * ProcAPI::
nextInstance ( PERF_INSTANCE_DEFINITION *pInstance ) {
  
        // next instance is after this instance + this instances counter data
    PERF_COUNTER_BLOCK *pCtrBlk;
    
    pCtrBlk = ( PERF_COUNTER_BLOCK *)
        ((PBYTE) pInstance + pInstance->ByteLength);
    
    return ( PERF_INSTANCE_DEFINITION * ) ((PBYTE) pInstance + 
                                           pInstance->ByteLength + 
                                           pCtrBlk->ByteLength  );
}

/* End pointer manip thingies */

/* This function serves to get the offsets of all the offsets for the 
   process counters.  These offsets tell where in the data block the
   counters for each instance can be found.
*/
void ProcAPI::grabOffsets ( PPERF_OBJECT_TYPE pThisObject ) {
  
    PPERF_COUNTER_DEFINITION pThisCounter;
  
    offsets = (struct Offset*) malloc ( sizeof ( struct Offset ));

    pThisCounter = firstCounter(pThisObject);
//    printcounter ( stdout, pThisCounter );
    offsets->pctcpu = pThisCounter->CounterOffset; // % cpu
    
    pThisCounter = nextCounter(pThisCounter);
//    printcounter ( stdout, pThisCounter );
    offsets->utime = pThisCounter->CounterOffset;  // % user time
  
    pThisCounter = nextCounter(pThisCounter);
//    printcounter ( stdout, pThisCounter );
    offsets->stime = pThisCounter->CounterOffset;  // % sys time
  
    pThisCounter = nextCounter(pThisCounter);
    pThisCounter = nextCounter(pThisCounter);
//    printcounter ( stdout, pThisCounter );
    offsets->imgsize = pThisCounter->CounterOffset;  // image size
  
    pThisCounter = nextCounter(pThisCounter);
//    printcounter ( stdout, pThisCounter );
    offsets->faults = pThisCounter->CounterOffset;   // page faults
    
    pThisCounter = nextCounter(pThisCounter);
    offsets->rssize = pThisCounter->CounterOffset;   // working set peak 
//    printcounter ( stdout, pThisCounter );
	pThisCounter = nextCounter(pThisCounter);		 // working set
	pThisCounter = nextCounter(pThisCounter);

    
    pThisCounter = nextCounter(pThisCounter);
    pThisCounter = nextCounter(pThisCounter);
    pThisCounter = nextCounter(pThisCounter);
    pThisCounter = nextCounter(pThisCounter);
    pThisCounter = nextCounter(pThisCounter);
//    printcounter ( stdout, pThisCounter );
    offsets->elapsed = pThisCounter->CounterOffset;  // elapsed time (age)
    
    pThisCounter = nextCounter(pThisCounter);
//    printcounter ( stdout, pThisCounter );
    offsets->procid = pThisCounter->CounterOffset;   // process id
}


int ProcAPI::isinlist( pid_t pid, pid_t *pidlist, int numpids ) {
	bool found = false;
	int i = 0;
    
	while ( ( !found ) && ( i < numpids ) ) {
		if ( pidlist[i] == pid )
			found = true;
		else
            i++;
	}
    
	if ( !found ) {
		return -1;
	}
	else {
		return i;
	}
}

/* This function exists to convert a LARGE_INTEGER into a double.  This is
   Needed because several numbers returned by my performance monitoring stuff
   are in the LARGE_INTEGER format, which is a struct of two longs, the LowPart
   and the HighPart.  I could have done something fancier, but just hacking
   everything into a double seems like the simplest thing to do.
 */
double ProcAPI::LI_to_double ( LARGE_INTEGER bigun ) {
  
  double ret;
  ret = (double) bigun.LowPart;
  ret += ( ((double) bigun.HighPart) * ( ((double)0xffffffff) + 1.0 ) ) ;
  return ret;
}

DWORD ProcAPI::GetSystemPerfData ( LPTSTR pValue ) 
{
  // Allocates data buffer as required and queries performance
  // data specified in Pvalue from registry
  
  /*  
      pValue      : Value string to return from registry
                    '230' is 'Process'.

      pDataBlock   : address of pointer to allocated perf data block that 
                    is filled in by call to RegQueryValue
                    NOTE : this is a class data member.
                    
      return      : error/ok
  */
  
  LONG lError;
  DWORD Size;    // size of data buffer passed to fn. call
  DWORD Type;    // type ....
  
  if ( pDataBlock == NULL) {
    pDataBlock = (PPERF_DATA_BLOCK) malloc ( INITIAL_SIZE );
    if ( pDataBlock == NULL )
      return ERROR_OUTOFMEMORY;
  }
  
  while ( TRUE ) {
    Size = _msize ( pDataBlock ); 
    
    lError = RegQueryValueEx ( HKEY_PERFORMANCE_DATA, pValue, 0, &Type, 
             (LPBYTE) pDataBlock, &Size );
    
    // check for success & valid perf data bolck struct.
    
    if ( (!lError) && (Size>0) && 
         (pDataBlock)->Signature[0] == (WCHAR)'P' &&
         (pDataBlock)->Signature[1] == (WCHAR)'E' &&
         (pDataBlock)->Signature[2] == (WCHAR)'R' &&
         (pDataBlock)->Signature[3] == (WCHAR)'F' )
      return ERROR_SUCCESS;
    
    // if buffer not big enough, reallocate and try again.
    
    if ( lError == ERROR_MORE_DATA ) {
      pDataBlock = (PPERF_DATA_BLOCK) realloc ( pDataBlock, 
                                                _msize (pDataBlock ) + 
                                                EXTEND_SIZE );
      if ( !pDataBlock)
        return lError;
    }
    else
      return lError;
  }
}
#endif // WIN32


#ifdef WANT_STANDALONE_DEBUG
void ProcAPI::runTests() {

  int ans;

  while (1) {

    printf ( "Test | Description\n" );
    printf ( "----   -----------\n" );
    printf ( " 1     Simple fork; monitor processes & family.\n" );
    printf ( " 2     Complex fork; monitor family.\n" );
    printf ( " 3     Determines if you can look at procs you don't own.\n");
    printf ( " 4     Tests procSetInfo...asks for pids, returns info.\n");
    printf ( " 5     Tests getPidFamily...forks kids & finds them again.\n");
    printf ( " 6     Tests cpu usage over time.\n");
    printf ( " 7     Fork a process; monitor it.  There's no return.\n");
    printf ( " 8     Exit.\n\n");
    printf ( "Please enter your choice: " );
    fflush ( stdout );
    scanf ( "%d", &ans );
    
    switch ( ans ) {
    case 1: 
      test1();
      break;
    case 2: 
      test2();
      break;
    case 3: 
      test3();
      break;
    case 4: 
      test4();
      break;
    case 5: 
      test5();
      break;
    case 6: 
      test6();
      break;
    case 7: 
      char file[128];
      printf ("Enter executable name:\n");
      scanf ("%s", file);
      test_monitor( file );
      break;
    case 8:
      exit(1);
    default:
      exit(1);
    }
    printf ( "\n\n\n" );
  }
}

void ProcAPI::sprog1 ( pid_t *childlist, int numkids, int f ) {
  
  pid_t child;
  int i, j;
  char **big;

  for ( i = 0; i < numkids ; i++ ) {
    child = fork();
    if ( !child ) {  // in child 
      printf("Child process # %d started....\n", i );
      fflush (stdout);

      big = new char*[1024*(i+1)*f];
      for ( j = 0 ; j < 1024*(i+1)*f ; j++ ) {
        big[j] = new char[1024];
        if ( big[j] == NULL ) {
          printf("Shit, new failed in child %i.  j=%i\n", i, j);
        }
      }

      printf ( "Child %d done allocating %d megs of memory.\n", i, (i+1)*f );

      for ( j=0 ; j < 1024*(i+1)*f ; j++ ) {
        big[j][0] = 'x';
      }

      printf ( "Done touching all pages in child %d\n", i );

      // let's do some work in the child, to get the cpu time up:
      for ( j=0 ; j < 100000000 ; j++ ) 
        ;

      printf ( "Done doing meaningless work in child %d\n", i );

      sleep ( 60 );   // take a nap for one minute - allows measurement.

      // now we deallocate what we allocated, to be nice.  :-)
      for ( j = 0 ; j < 1024*(i+1)*f ; j++ ) {  // deallocation
        delete [] big[j];
      }
      delete [] big;

      exit(1);
    }
    else {
      printf("In parent.  Child process #%d (pid=%d) created.\n", i, child );
      childlist[i] = child;
    }
  }
}

void ProcAPI::sprog2( int numkids, int f ) {

  pid_t child;
  char **big;
  int i = numkids;
  int j;
  
  if ( numkids == 0 )   // bail out when we're done. (stop the recursion!)
    return;

  child = fork();
  if ( !child ) { // in child
    sprog2 ( numkids-1, f );

    big = new char*[1024*i*f];
    for ( j = 0 ; j < 1024*i*f ; j++ ) {
      big[j] = new char[1024];
      if ( big[j] == NULL ) {
        printf("Shit, new failed in child %i.  j=%i\n", i, j);
      }
    }

    printf ( "Child %d done allocating %d megs of memory.\n", i, i*f );
    
    for ( j=0 ; j < 1024*i*f ; j++ ) {
      big[j][0] = 'x';
    }
    
    printf ( "Done touching all pages in child %d\n", i );
    
    // let's do some work in the child, to get the cpu time up:
    for ( j=0 ; j < 100000000 ; j++ ) 
      ;
    
    printf ( "Done doing meaningless work in child %d\n", i );
    
    sleep ( 60 );   // take a nap for one minute - allows measurement.
    
    // now we deallocate what we allocated, to be nice.  :-)
    for ( j = 0 ; j < 1024*i*f ; j++ ) {  // deallocation
      delete [] big[j];
    }
    delete [] big;
    
    exit(1);
  }
}

void ProcAPI::test1() {

  pid_t children[NUMKIDS];
  piPTR pi = NULL;

  printf ( "\n..................................\n" );
  printf ( "This first test demonstrates the creation and monitoring of\n" );
  printf ( "a simple family of processes.  This process forks off %d copies\n",
           NUMKIDS );
  printf ( "of itself, and each of these children allocate some memory,\n" );
  printf ( "touch each page of it, do some cpu-intensive work, and then\n" );
  printf ( "deallocate the memory they allocated. They then sleep for a\n" );
  printf ( "minute and exit.  These children are monitored individually\n" );
  printf ( "and as a family of processes.\n\n" );

  sprog1 ( children, NUMKIDS, MEMFACTOR );

  printf ( "Parent:  sleeping 30 secs to let kids play awhile.\n" );
  // take a break to let children work
  for ( int i=0 ; i < 30 ; i+=5 ) {
    printf ( "." );
    fflush(stdout);
    sleep ( 5 );
  }

  printf ( "\n" );

  for ( int i=0 ; i < NUMKIDS ; i++ ) {
    printf ( "Info for Child #%d, pid=%d...", i, children[i] );
    printf ( "should have allocated %d Megs.\n", (i+1)*MEMFACTOR );
    getProcInfo ( children[i], pi );
    printProcInfo( pi );
  }
  printf ( "Parental info: pid:%d\n", getpid() );
  getProcInfo ( getpid(), pi );
  printProcInfo( pi );

  printf ( "Now get the information of this pid and all its descendents:\n" );
  printf ( "Total allocated memory should be around %d Megs.\n", 
           (NUMKIDS*(NUMKIDS+1))/2 * MEMFACTOR );

  printf ( "Family info for pid: %d\n", getpid() );
  if ( getFamilyInfo( getpid(), pi ) < 0 )
    printf ( "Unable to get info.  That shouldn't have happened.\n");
  else
    printProcInfo( pi );

  delete pi;
}

void ProcAPI::test2() {
  piPTR pi = NULL;
  pid_t child;

  printf ( "\n....................................\n" );
  printf ( "This test is similar to test 1, except that the children are\n");
  printf ( "forked off in a different way.  In this case, the parent forks\n");
  printf ( "a child, which forks a child, which forks a child, etc.  It\n");
  printf ( "should still be possible to monitor this group by getting the\n");
  printf ( "family info for the parent pid.  Let's see...\n\n");

  child = fork();
  if ( !child ) { // in child 
    sprog2 ( NUMKIDS, 1 );
    sleep(60);
    exit(1);
  }

  printf ( "Parent:  sleeping 30 secs to let kids play awhile.\n" );
  // take a break to let children work
  for ( int i=0 ; i < 30 ; i+=5 ) {
    printf ( "." );
    fflush(stdout);
    sleep ( 5 );
  }
  printf ( "\n" );
  
  printf ( "Total allocated memory should be around %d Megs.\n", 
           (NUMKIDS*(NUMKIDS+1))/2 );

  printf ( "Family info for pid: %d\n", child );
  if ( getFamilyInfo( child, pi ) < 0 )
    printf ( "Unable to get info.  That shouldn't have happened.\n");
  else
    printProcInfo( pi );
  
  delete pi;  
}

void ProcAPI::test3() {

  piPTR pi = NULL;

  printf ( "\n..................................\n" );
  printf ( "This test determines if you can get information on processes\n" );
  printf ( "other than those you own.  If you get a summary of all the\n" );
  printf ( "processes in the system ( parent=1 ) then you can.\n\n" );

  if ( getFamilyInfo( 1, pi ) < 0 )
    printf ( "Unable to get info.  Try becoming root.  :-)\n");
  else
    printProcInfo( pi );

  delete pi;
}

void ProcAPI::test4() {

  piPTR pi = NULL;

  int numpids;
  pid_t *pids;

  printf ( "\n..................................\n" );
  printf ( "Test 4 is a quick test of getProcSetInfo().  First enter the\n");
  printf ( "number of processes to look at, then individual pids.\n");
  printf ( "# pids -> " );
  scanf ( "%d", &numpids );
  printf ( "\nPids -> " );

  pids = new pid_t[numpids];
  for ( int i=0 ; i < numpids ; i++ ) {
    scanf ( "%d", &pids[i] );
  }

  getProcSetInfo ( pids, numpids, pi );
  
  printf ( "\n\nThe totals are:\n" );
  printProcInfo ( pi );

  delete pi;
  delete [] pids;
}

void ProcAPI::test5() {
  
  piPTR pi = NULL;

  printf ( "\n..................................\n" );
  printf ( "This is a quick test of getPidFamily().  It forks off some\n" );
  printf ( "processes and it'll print out the pids associated with\n" );
  printf ( "these children.\n" );

  int child;

  child = fork();
  if ( !child ) { // in child
    printf ( "Child %d created.\n", getpid() );    
    child = fork();
    if ( !child ) { // in child
      printf ( "Child %d created.\n", getpid() );    
      child = fork();
      if ( !child ) { // in child
        printf ( "Child %d created.\n", getpid() );    
        sleep(20);
        exit(1);
      }
      else {
        sleep(20);
        exit(1);
      }
    }
    else {
      sleep(20);
      exit(1);
    }
  }
  else { // in parent
    sleep (10);
    pid_t *pidf = new pid_t[20];
    getPidFamily( getpid(), pidf );
    printf ( "Result of getPidFamily...the descendants are:\n" );

    for ( int i=0 ; pidf[i] != 0 ; i++ ) {
      printf ( " %d ", pidf[i] );
    }

	delete [] pidf;

    printf ( "\n" );

    printf ( "If the above results didn't match, I f*cked up.\n" );
  }
}

void ProcAPI::test6() {

  piPTR pi = NULL;
  pid_t child;

  printf ( "\n..................................\n" );
  printf ( "Here's the test of cpu usage monitoring over time.  I'll\n");
  printf ( "fork off a process, which will alternate between sleeping\n");
  printf ( "and working.  I'll return info on that process.\n");
  printf ( "This test runs for one minute.\n");

  child = fork();
  if ( !child ) { // in child
    for ( int i=0 ; i<4 ; i++ ) {
      char * poo = new char[1024*1024];
      printf ( "Child working.\n");
      for ( int j=0 ; j < 200000000 ; j++ )
        ;
      printf ( "Child sleeping.\n");
      sleep(10);
    }
    exit(1);
  }
  else {
    for ( int i=0 ; i<12 ; i++ ) {
      sleep(5);
      printf ("\n");
      getProcInfo ( child, pi );
      printProcInfo ( pi );
    }
  }
}

void ProcAPI::test_monitor ( char * jobname ) {
  pid_t child;
  int rval;
  piPTR pi = NULL;

  printf ( "Here's the interesting test.  This one does a fork()\n");
  printf ( "and then an exec() of the name of the program passed to\n");
  printf ( "it.  In this case, that's %s.\n", jobname );
  printf ( "This monitoring program will wake up every 10 seconds and\n");
  printf ( "spit out a report.\n");

  child = fork();
  
  if ( !child ) { // in child
    rval = execl( jobname, jobname, (char*)0 );
    if ( rval < 0 ) {
      perror ( "Exec problem:" );
      exit(0);
    }
  }

  while ( 1 ) {
    sleep ( 10  );
    if ( getFamilyInfo( child, pi ) < 0 ) {
      printf ( "Problem getting information.  Exiting.\n");
      exit(1);
    }
    else {
      printProcInfo ( pi );
    }
  }

  delete pi;  // we'll never get here, but oh well...:-)

}
#endif  // of WANT_STANDALONE_DEBUG
