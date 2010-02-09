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
#include "condor_debug.h"
#include "procapi.h"
#include "procapi_internal.h"

// Ugly hack: stat64 prototyps are wacked on HPUX
// These are cut & pasted from the HPUX man pages...                            
#if defined( HPUX )
extern "C" {
    extern int fstat64(int fildes, struct stat64 *buf);
}
#endif


unsigned int pidHashFunc( const pid_t& pid );

HashTable <pid_t, procHashNode *> * ProcAPI::procHash = 
    new HashTable <pid_t, procHashNode *> ( PHBUCKETS, pidHashFunc );

piPTR ProcAPI::allProcInfos = NULL;

#ifndef WIN32
pidlistPTR ProcAPI::pidList = NULL;
int ProcAPI::pagesize		= 0;
#ifdef LINUX
long unsigned ProcAPI::boottime	= 0;
long ProcAPI::boottime_expiration = 0;
#endif // LINUX
#else // WIN32

#include "ntsysinfo.h"
static CSysinfo ntSysInfo;	// for getting parent pid on NT

	// Windows gives us birthday in 100ns ticks since 01/01/1601
	// (11644473600 seconds before 01/01/1970)
const __int64 EPOCH_SHIFT = 11644473600;

PPERF_DATA_BLOCK ProcAPI::pDataBlock	= NULL;
struct Offset * ProcAPI::offsets	= NULL;

#endif // WIN32

int ProcAPI::MAX_SAMPLES = 5;
int ProcAPI::DEFAULT_PRECISION_RANGE = 4;

#if defined(HZ)
double ProcAPI::TIME_UNITS_PER_SEC = (double) HZ;
#elif defined(_SC_CLK_TCK)
double ProcAPI::TIME_UNITS_PER_SEC = (double) sysconf(_SC_CLK_TCK);
#else // not linux
double ProcAPI::TIME_UNITS_PER_SEC = 1.0;
#endif

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
#endif
    deallocAllProcInfos();

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

// Each platform gets its own function unless two are so similar that you can
// ifdef between them.

// this version works for Solaris 2.5.1, IRIX, OSF/1 
#if ( defined(Solaris251) || defined(IRIX) || defined(OSF1) )
int
ProcAPI::getProcInfo( pid_t pid, piPTR& pi, int &status ) 
{

		// This *could* allocate memory and make pi point to it if pi == NULL.
		// It is up to the caller to get rid of it.
	initpi( pi );

		// get the raw system process data
	procInfoRaw procRaw;
	int retVal = ProcAPI::getProcInfoRaw(pid, procRaw, status);
	
		// if a failure occurred
	if( retVal != 0 ){
			// return failure
			// status is set by getProcInfoRaw(...)
		return PROCAPI_FAILURE;
	}

		/* clean up and convert the raw data */

		// if the page size has not yet been found, get it.
	if( pagesize == 0 ) {
		pagesize = getpagesize() / 1024;  // pagesize is in k now
	} 

		// convert the memory from pages to k
	pi->imgsize = procRaw.imgsize * pagesize;
	pi->rssize  = procRaw.rssize * pagesize;

		// compute the age
	pi->age = procRaw.sample_time - procRaw.creation_time;

		// sanity check user and system times
		// and compute cpu time
	pi->user_time = procRaw.user_time_1;
	pi->sys_time = procRaw.sys_time_1;
	
		// converts _2 times into seconds and
		// adds to _1 (already in seconds) times
	double cpu_time = 	
		( procRaw.user_time_1 + 
		  ( procRaw.user_time_2 * 1.0e-9 ) ) +
		( procRaw.sys_time_1 + 
		  ( procRaw.sys_time_2 * 1.0e-9 ) );
	
		/* The bastard os lies to us and can return a negative number for stime.
		   ps returns the wrong number in its listing, so this is an IRIX bug
		   that we cannot work around except this way */
#if defined(IRIX)
	if (pi->user_time < 0)
		pi->user_time = 0;
	
	if (pi->sys_time < 0)
		pi->sys_time = 0;

	if(cpu_time < 0)
		cpu_time = 0.0;	
#endif	

		// copy the remainder of the fields
	pi->pid     = procRaw.pid;
	pi->ppid    = procRaw.ppid;
	pi->creation_time = procRaw.creation_time;
	pi->birthday = procRaw.creation_time;
	pi->owner = procRaw.owner;


    /* here we've got to do some sampling ourself.  If the pid is not in
       the hashtable, put it there using cpu_time / age as %cpu.
       If it is there, use (cpu_time - old time) / timediff.
    */
	do_usage_sampling( pi, cpu_time, procRaw.majfault, procRaw.minfault);

		// success
	return PROCAPI_SUCCESS;
}

/* Fills the struct with the following units:
   imgsize		: pages
   rssize		: pages
   minfault		: minor faults since born
   majfault		: minor faults since born 
   user_time_1	: seconds
   user_time_2	: nanoseconds
   sys_time_1	: seconds
   sys_time_2	: nanoseconds
   creation_time: seconds since epoch
   sample_time	: seconds since epoch
*/

int
ProcAPI::getProcInfoRaw(pid_t pid, procInfoRaw& procRaw, int& status){
	char path[64];
	struct prpsinfo pri;
	struct prstatus prs;
#ifndef DUX4
	struct prusage pru;   // prusage doesn't exist in OSF/1
#endif

	int fd;

		// assume success
	status = PROCAPI_OK;

		// clear the memory for procRaw
	initProcInfoRaw(procRaw);

		// set the sample time
	procRaw.sample_time = secsSinceEpoch();

		// open /proc/<pid>
	sprintf( path, "/proc/%d", pid );
	if( (fd = safe_open_wrapper(path, O_RDONLY)) < 0 ) {
		switch(errno) {

			case ENOENT:
				// pid doesn't exist
				status = PROCAPI_NOPID;
				dprintf( D_FULLDEBUG, "ProcAPI: pid %d does not exist.\n",
						pid );
				break;

			case EACCES:
				status = PROCAPI_PERM;
				dprintf( D_FULLDEBUG, "ProcAPI: No permission to open %s.\n", 
					 	path );
				break;

			default:
				status = PROCAPI_UNSPECIFIED;
				dprintf( D_ALWAYS, "ProcAPI: Error opening %s, errno: %d.\n", 
					 path, errno );
				break;
		}

		return PROCAPI_FAILURE;
	}


	// PIOCPSINFO gets memory sizes, pids, and age.
	if ( ioctl( fd, PIOCPSINFO, &pri ) < 0 ) {
		dprintf( D_ALWAYS, "ProcAPI: PIOCPSINFO Error occurred for pid %d\n",
				 pid );

		close( fd );

		status = PROCAPI_UNSPECIFIED;
		return PROCAPI_FAILURE;
	}

	// grab out the information 
	procRaw.imgsize = pri.pr_size;
	procRaw.rssize  = pri.pr_rssize;
	procRaw.pid     = pri.pr_pid;
	procRaw.ppid    = pri.pr_ppid;
	procRaw.creation_time = pri.pr_start.tv_sec;

	// get the owner of the file in /proc, which 
	// should be the process owner uid.
	procRaw.owner = getFileOwner(fd);			

    // PIOCUSAGE is used for page fault info
    // solaris 2.5.1 and Irix only - unsupported by osf/1 dux-4
    // Now in DUX5, though...
#ifndef DUX4
	if ( ioctl( fd, PIOCUSAGE, &pru ) < 0 ) {
		dprintf( D_ALWAYS, 
				 "ProcAPI: PIOCUSAGE Error occurred for pid %d\n", 
				 pid );

		close( fd );
		
		status = PROCAPI_UNSPECIFIED;
		return PROCAPI_FAILURE;
	}

#ifdef Solaris251   
	procRaw.minfault = pru.pr_minf;  
	procRaw.majfault = pru.pr_majf;  
#endif // Solaris251

#ifdef IRIX   // dang things named differently in irix.
	procRaw.minfault = pru.pu_minf;  // Irix:  pu_minf, pu_majf.
	procRaw.majfault = pru.pu_majf;
#endif // IRIX
	
#else  // here we are in osf/1, which doesn't give this info.

	procRaw.minfault = 0;   // let's default to zero in osf1
	procRaw.majfault = 0;

#endif // DUX4

   // PIOCSTATUS gets process user & sys times
   // this following bit works for Sol 2.5.1, Irix, Osf/1
	if ( ioctl( fd, PIOCSTATUS, &prs ) < 0 ) {
		dprintf( D_ALWAYS, 
				 "ProcAPI: PIOCSTATUS Error occurred for pid %d\n", 
				 pid );

		close ( fd );

		status = PROCAPI_UNSPECIFIED;
		return PROCAPI_FAILURE;
	}

	procRaw.user_time_1 = prs.pr_utime.tv_sec;
	procRaw.user_time_2 = prs.pr_utime.tv_nsec;
	procRaw.sys_time_1 = prs.pr_stime.tv_sec;
	procRaw.sys_time_2 = prs.pr_stime.tv_nsec;

	// close the /proc/pid file
	close ( fd );

	return PROCAPI_SUCCESS;
}

#elif defined(Solaris26) || defined(Solaris27) || defined(Solaris28) || defined(Solaris29)
// This is the version of getProcInfo for Solaris 2.6 and 2.7 and 2.8 and 2.9

int
ProcAPI::getProcInfo( pid_t pid, piPTR& pi, int &status ) 
{
	// This *could* allocate memory and make pi point to it if pi == NULL.
	// It is up to the caller to get rid of it.
	initpi ( pi );

		// assume sucess
	status = PROCAPI_OK;

	// get the raw system process data
	procInfoRaw procRaw;
	int retVal = ProcAPI::getProcInfoRaw(pid, procRaw, status);
	
		// if a failure occured
	if( retVal != 0 ){
			// return failure
			// status is set by getProcInfoRaw(...)
		return PROCAPI_FAILURE;
	}

		/* clean up and convert raw data */

		// compute the age
	pi->age = procRaw.sample_time - procRaw.creation_time;

		// compute cpu time
	double cpu_time = 
		( procRaw.user_time_1 + 
		  (procRaw.user_time_2 * 1.0e-9) ) +
		( procRaw.sys_time_1 + 
		  (procRaw.sys_time_2 * 1.0e-9) ); 


		// copy the remainder of the fields
	pi->pid		= procRaw.pid;
	pi->ppid	= procRaw.ppid;
	pi->owner = procRaw.owner;
	pi->creation_time = procRaw.creation_time;
	pi->birthday    = procRaw.creation_time;
	pi->imgsize	= procRaw.imgsize;    // already in k!
	pi->rssize	= procRaw.rssize;  // already in k!
	pi->user_time= procRaw.user_time_1;
	pi->sys_time = procRaw.sys_time_1;
	
  /* Now we do that sampling hashtable thing to convert page faults
     into page faults per second */
	do_usage_sampling( pi, cpu_time, procRaw.majfault, procRaw.minfault );

		// success
	return PROCAPI_SUCCESS;
}

/* Fills ProcInfoRaw with the following units:
   imgsize		: KB
   rssize		: KB
   minfault		: total minor faults
   majfault		: total major faults
   user_time_1	: seconds
   user_time_2	: nanos
   sys_time_1	: seconds
   sys_time_2	: nanos
   creation_time: seconds since epoch
   sample_time	: seconds since epoch

*/

int
ProcAPI::getProcInfoRaw( pid_t pid, procInfoRaw& procRaw, int &status ) 
{
	char path[64];
	int fd;
	psinfo_t psinfo;
	prusage_t prusage;

		// assume success
	status = PROCAPI_OK;

		// clear the memory of procRaw
	initProcInfoRaw(procRaw);

		// set the sample time
	procRaw.sample_time = secsSinceEpoch();

		// pids, memory usage, and age can be found in 'psinfo':
	sprintf( path, "/proc/%d/psinfo", pid );
	if( (fd = safe_open_wrapper(path, O_RDONLY)) < 0 ) {

		switch(errno) {
			case ENOENT:
				// pid doesn't exist
				status = PROCAPI_NOPID;
				dprintf( D_FULLDEBUG, "ProcAPI: pid %d does not exist.\n",
						pid );
				break;

			case EACCES:
				status = PROCAPI_PERM;
				dprintf( D_FULLDEBUG, "ProcAPI: No permission to open %s.\n", 
					 	path );
				break;

			default:
				status = PROCAPI_UNSPECIFIED;
				dprintf( D_ALWAYS, "ProcAPI: Error opening %s, errno: %d.\n", 
					 path, errno );
				break;
		}

		return PROCAPI_FAILURE;
	} 

	// grab the information from the file descriptor.
	if( read(fd, &psinfo, sizeof(psinfo_t)) != sizeof(psinfo_t) ) {
		dprintf( D_ALWAYS, 
			"ProcAPI: Unexpected short read while reading %s.\n", path );

		status = PROCAPI_GARBLED;

		return PROCAPI_FAILURE;
	}

	// grab the process owner uid
	procRaw.owner = getFileOwner(fd);

	close( fd );

	// grab the information out of what the kernel told us. 
	procRaw.imgsize	= psinfo.pr_size;
	procRaw.rssize	= psinfo.pr_rssize;
	procRaw.pid		= psinfo.pr_pid;
	procRaw.ppid	= psinfo.pr_ppid;
	procRaw.creation_time = psinfo.pr_start.tv_sec;

  // maj/min page fault info and user/sys time is found in 'usage':
  // I have never seen minor page faults return anything 
  // other than '0' in 2.6.  I have seen a value returned for 
  // major faults, but not that often.  These values are suspicious.
	sprintf( path, "/proc/%d/usage", pid );
	if( (fd = safe_open_wrapper(path, O_RDONLY) ) < 0 ) {

		switch(errno) {
			case ENOENT:
				// pid doesn't exist
				status = PROCAPI_NOPID;
				dprintf( D_FULLDEBUG, "ProcAPI: pid %d does not exist.\n",
						pid );
				break;

			case EACCES:
				status = PROCAPI_PERM;
				dprintf( D_FULLDEBUG, "ProcAPI: No permission to open %s.\n", 
					 	path );
				break;

			default:
				status = PROCAPI_UNSPECIFIED;
				dprintf( D_ALWAYS, "ProcAPI: Error opening %s, errno: %d.\n", 
					 path, errno );
				break;
		}

		return PROCAPI_FAILURE;

	}

	if( read(fd, &prusage, sizeof(prusage_t)) != sizeof(prusage_t) ) {

		dprintf( D_ALWAYS, 
			"ProcAPI: Unexpected short read while reading %s.\n", path );

		status = PROCAPI_GARBLED;

		return PROCAPI_FAILURE;
	}

	close( fd );

	procRaw.minfault = prusage.pr_minf;
	procRaw.majfault = prusage.pr_majf;
	procRaw.user_time_1 = prusage.pr_utime.tv_sec;
	procRaw.user_time_2 = prusage.pr_utime.tv_nsec;
	procRaw.sys_time_1 = prusage.pr_stime.tv_sec;
	procRaw.sys_time_2 = prusage.pr_stime.tv_nsec;

	return PROCAPI_SUCCESS;
}

#elif defined(LINUX)

int
ProcAPI::getProcInfo( pid_t pid, piPTR& pi, int &status ) 
{


	// This *could* allocate memory and make pi point to it if pi == NULL.
	// It is up to the caller to get rid of it.
	initpi( pi );

		// get the raw system process data
	procInfoRaw procRaw;
	int retVal = ProcAPI::getProcInfoRaw(pid, procRaw, status);
	
		// if a failure occurred
	if( retVal != 0 ){
			// return failure
			// status is set by getProcInfoRaw(...)
		return PROCAPI_FAILURE;
	}

		/* clean up and convert the raw data */

		// if the page size has not yet been found, get it.
	if( pagesize == 0 ) {
		pagesize = getpagesize() / 1024;
	}

		/*
		  Zero out thread memory, because Linux (as of kernel 2.4)
		  shows one process per thread, with the mem stats for each
		  thread equal to the memory usage of the entire process.
		  This causes ImageSize to be far bigger than reality when
		  there are many threads, so if the job gets evicted, it might
		  never be able to match again.

		  There is no perfect method for knowing if a given process
		  entry is actually a thread.  One way is to compare the
		  memory usage to the parent process, and if they are
		  identical, it is probably a thread.  However, there is a
		  small race condition if one of the entries is updated
		  between reads; this could cause threads not to be weeded out
		  every now and then, which can cause the ImageSize problem
		  mentioned above.

		  So instead, we use the PF_FORKNOEXEC (64) process flag.
		  This is always turned on in threads, because they are
		  produced by fork (actually clone), and they continue on from
		  there in the same code, i.e.  there is no call to exec.  In
		  some rare cases, a process that is not a thread will have
		  this flag set, because it has not called exec, and it was
		  created by a call to fork (or equivalently clone with
		  options that cause memory not to be shared).  However, not
		  only is this rare, it is not such a lie to zero out the
		  memory usage, because Linux does copy-on-write handling of
		  the memory.  In other words, memory is only duplicated when
		  the forked process writes to it, so we are once again in
		  danger of over-counting memory usage.  When in doubt, zero
		  it out!

		  One exception to this rule is made for processes inherited
		  by init (ppid=1).  These are clearly not threads but are
		  background processes (such as condor_master) that fork and
		  exit from the parent branch.
		*/
	pi->imgsize = procRaw.imgsize / 1024;  //bytes to k
	pi->rssize = procRaw.rssize * pagesize;  // pages to k
	if ((procRaw.proc_flags & 64) && procRaw.ppid != 1) { //64 == PF_FORKNOEXEC
		//zero out memory usage
		pi->imgsize = 0;
		pi->rssize = 0;
	}

		// convert system time and user time into seconds from jiffies
		// and calculate cpu time
	long	hertz;
# if defined(HZ)
	hertz = HZ;
# elif defined(_SC_CLK_TCK)
	hertz = sysconf(_SC_CLK_TCK);
# else
#   error "Unable to determine system clock"
# endif
	pi->user_time   = procRaw.user_time_1 / hertz;
	pi->sys_time    = procRaw.sys_time_1  / hertz;
	double cpu_time = (procRaw.user_time_1 + procRaw.sys_time_1) / (double)hertz;

		// use the raw value (which is in jiffies since boot time)
		// as the birthday, which is useful for comparing whether
		// two processes with the same  PID are in fact the same
		// process
	pi->birthday = procRaw.creation_time;

		// convert the creation time from jiffies since boot
		// to epoch time

		// 1. ensure we have a "good" boottime
	if( checkBootTime(procRaw.sample_time) == PROCAPI_FAILURE ){
		status = PROCAPI_UNSPECIFIED;
		dprintf( D_ALWAYS, "ProcAPI: Problem getting boottime\n");
		return PROCAPI_FAILURE;
	}
	
		// 2. Convert from jiffies to seconds
	long bday_secs = procRaw.creation_time / hertz;
	
		// 3. Convert from seconds since boot to seconds since epoch
	pi->creation_time = bday_secs + boottime;

		// calculate age
	pi->age = procRaw.sample_time - pi->creation_time;

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
	
	
		// copy the remainder of the fields
	pi->owner = procRaw.owner;
	pi->pid = procRaw.pid;
	pi->ppid = procRaw.ppid;

		/* here we've got to do some sampling ourself to get the
		   cpuusage and make the page faults a rate...
		*/
	do_usage_sampling ( pi, cpu_time, procRaw.majfault, procRaw.minfault );
		// Note: sanity checking done in above call.

		/* grab out the environment, if possible. I've noticed that
		   under linux it appears that once the /proc/<pid>/environ
		   file is made, it never changes. Luckily, we're only looking
		   for specific stuff the parent only puts into the child's
		   environment.

		   We don't care if it fails, its optional
		*/
	fillProcInfoEnv(pi);

		// success
	return PROCAPI_SUCCESS;
}

/* Fills in procInfoRaw with the following units:
   imgsize		: bytes
   rssize		: pages
   minfault		: total minor faults
   majfault		: total major faults
   user_time_1	: jiffies (1/100 of a second)
   user_time_2	: not set
   sys_time_1	: jiffies (1/100 of a second)
   sys_time_2	: not set
   creation_time: jiffies since system boot
   sample_time	: seconds since epoch
   proc_flags	: special process flags
*/
int
ProcAPI::getProcInfoRaw( pid_t pid, procInfoRaw& procRaw, int &status ) 
{

// This is the Linux version of getProcInfoRaw.  Everything is easier and
// actually seems to work in Linux...nice, but annoyingly different.

	char path[64];
	FILE *fp;
	
	int number_of_attempts;
	long i;
	unsigned long u;
	char c;
	char s[256];
	int num_attempts = 5;

		// assume success
	status = PROCAPI_OK;

		// clear the memory of procRaw
	initProcInfoRaw(procRaw);

		// set the sample time
	procRaw.sample_time = secsSinceEpoch();

	// read the entry a certain number of times since it appears that linux
	// often simply does something stupid while reading.
	sprintf( path, "/proc/%d/stat", pid );
	number_of_attempts = 0;
	while (number_of_attempts < num_attempts) {

		number_of_attempts++;

		// in case I must restart, assume that everything is ok again...
		status = PROCAPI_OK;
		initProcInfoRaw(procRaw);

		if( (fp = safe_fopen_wrapper(path, "r")) == NULL ) {
			if( errno == ENOENT ) {
				// /proc/pid doesn't exist
				status = PROCAPI_NOPID;
				dprintf( D_FULLDEBUG, 
					"ProcAPI::getProcInfo() pid %d does not exist.\n", pid );
			} else if ( errno == EACCES ) {
				status = PROCAPI_PERM;
				dprintf( D_FULLDEBUG, 
					"ProcAPI::getProcInfo() No permission to open %s.\n", 
					 path );
			} else { 
				status = PROCAPI_UNSPECIFIED;
				dprintf( D_ALWAYS, 
					"ProcAPI::getProcInfo() Error opening %s, errno: %d.\n", 
					 path, errno );
			}
			
			// immediate failure, try again.
			continue;
		}

			// fill the raw structure from the proc file
			// ensure I read the right number of arguments....
		if ( fscanf( fp, "%d %s %c %d "
			"%ld %ld %ld %ld "
			"%lu %lu %lu %lu %lu "
			"%ld %ld %ld %ld %ld %ld "
			"%lu %lu %llu %lu %lu %lu %lu %lu %lu %lu %lu "
			"%ld %ld %ld %ld %lu",
			&procRaw.pid, s, &c, &procRaw.ppid, 
			&i, &i, &i, &i, 
			&procRaw.proc_flags, &procRaw.minfault, &u, &procRaw.majfault, &u, 
			&procRaw.user_time_1, &procRaw.sys_time_1, &i, &i, &i, &i, 
			&u, &u, &procRaw.creation_time, &procRaw.imgsize, &procRaw.rssize, &u, &u, &u, 
			&u, &u, &u, &i, &i, &i, &i, &u ) != 35 )
		{
			// couldn't read the right number of entries.
			status = PROCAPI_UNSPECIFIED;
			dprintf( D_ALWAYS, 
				"ProcAPI: Unexpected short scan on %s, errno: %d.\n", 
				 path, errno );

			// don't leak for the next attempt;
			fclose( fp );
			fp = NULL;

			// try again
			continue;
		}

		// do a small verification of the read in data...
		if ( pid == procRaw.pid ) {
			// end the loop, data looks ok.
			break;
		}
		
		// if we've made it here, pid != pi->pid, which tells
		// us that Linux screwed up and the info we just
		// read from /proc is screwed.  so set rval appropriately,
		// and we'll either try again or give up based on
		// number_of_attempts.
		status = PROCAPI_GARBLED;

	} 	// end of while number_of_attempts < 0

	// Make sure the data is good before continuing.
	if ( status != PROCAPI_OK ) {

		// This is a little wierd, so manually print this one out if
		// I got this far and only found garbage data
		if ( status == PROCAPI_GARBLED ) {
			dprintf( D_ALWAYS, 
				"ProcAPI: After %d attempts at reading %s, found only "
				"garbage! Aborting read.\n", num_attempts, path);
		}

		if (fp != NULL) {
			fclose( fp );
			fp = NULL;
		}

		return PROCAPI_FAILURE;
	}

	// grab the process owner uid
	procRaw.owner = getFileOwner(fileno(fp));

		// close the file
	fclose( fp );

		// only one value for times
	procRaw.user_time_2 = 0;
	procRaw.sys_time_2 = 0;
	
	return PROCAPI_SUCCESS;
}

int 
ProcAPI::fillProcInfoEnv(piPTR pi)
{
	char path[64];
	int read_size = (1024 * 1024);
	int bytes_read;
	int bytes_read_so_far = 0;
	int fd;
	char *env_buffer = NULL;
	int multiplier = 2;

		// open the environment proc file
	sprintf( path, "/proc/%d/environ", pi->pid );
	fd = safe_open_wrapper(path, O_RDONLY);

	// Unlike other things set up into the pi structure, this is optional
	// since it can only help us if it is here...
	if ( fd != -1 ) {
		
		// read the file in read_size chunks resizing the memory buffer
		// until I've read everything. I just can't read into a static
		// buffer since the user supplies the environment and I don't want
		// to produce a buffer overrun. However, you can't stat() this file
		// to see how big it is so I just have to keep reading until I stop.
		do {
			if (env_buffer == NULL) {
				env_buffer = (char*)malloc(sizeof(char) * read_size);
				if ( env_buffer == NULL ) {
					EXCEPT( "Procapi::getProcInfo: Out of memory!\n");
				}
			} else {
				env_buffer = (char*)realloc(env_buffer, read_size * multiplier);
				if ( env_buffer == NULL ) {
					EXCEPT( "Procapi::getProcInfo: Out of memory!\n");
				}
				multiplier++;
			}

			bytes_read = full_read(fd, env_buffer+bytes_read_so_far, read_size);
			bytes_read_so_far += bytes_read;

			// if I read right up to the end of the buffer size, assume more...
			// in the case of no more data, but this is true, then the buffer
			// will grow again, but no more will be read.
		} while (bytes_read == read_size);

		close(fd);

		// now convert the format, which are NUL delimited strings to the 
		// usual format of an environ.

		// count the entries
		int entries = 0;
		int index = 0;
		while(index < bytes_read_so_far) {
			if (env_buffer[index] == '\0') {
				entries++;
			}
			index++;
		}

		// allocate a buffer to hold pointers to the strings in env_buffer so 
		// it mimics and environ variable. The addition +1 is for the end NULL;
		char **env_environ;
		env_environ = (char**)malloc(sizeof(char *) * (entries + 1));
		if (env_environ == NULL) {
			EXCEPT( "Procapi::getProcInfo: Out of memory!\n");
		}

		// set up the pointers from the env_environ into the env_buffer
		index = 0;
		long i;
		for (i = 0; i < entries; i++) {
			env_environ[i] = &env_buffer[index];

			// find the start of the next entry
			while (index < bytes_read_so_far && env_buffer[index] != '\0') {
				index++;
			}

			// move over the \0 to the start of the next piece. At the end
			// of the buffer, this will point to garabge memory, but the for 
			// loop index will fire and this index won't be used.
			index++;
		}
	
		// the last entry must be NULL to simulate the environ variable.
		env_environ[i] = NULL;

		// if this pid happens to have any ancestor environment id variables,
		// then filter them out and put it into the PidEnvID table for this
		// proc. 
		if (pidenvid_filter_and_insert(&pi->penvid, env_environ) 
			== PIDENVID_OVERSIZED)
		{
			EXCEPT("ProcAPI::getProcInfo: Discovered too many ancestor id "
					"environment variables in pid %u. Programmer Error.\n",
					pi->pid);
		}

		// don't leak memory of the environ buffer
		free(env_buffer);
		env_buffer = NULL;

		// get rid of the container to simulate the environ convention
		free(env_environ);
		env_environ = NULL;
	}

	return PROCAPI_SUCCESS;
}

int
ProcAPI::checkBootTime(long now)
{

		// get the system boot time periodically, since it may change
		// if the time on the machine is adjusted
	if( boottime_expiration <= now ) {
		// There are two (sometimes conflicting) sources of boottime
		// information.  One is the /proc/stat "btime" field, and the
		// other is /proc/uptime.  For some unknown reason, btime
		// has been observed to suddenly jump forward in time to
		// the present moment, which totally messes up the process
		// age calculation, which messes up the CPU usage estimation.
		// Since this is not well understood, we hedge our bets
		// and use whichever measure of boot-time is older.

		FILE *fp;
		char s[256], junk[16];
		unsigned long stat_boottime = 0;
		unsigned long uptime_boottime = 0;

		// get uptime_boottime
		if( (fp = safe_fopen_wrapper("/proc/uptime","r")) ) {
			double uptime=0;
			double dummy=0;
			fgets( s, 256, fp );
			if (sscanf( s, "%lf %lf", &uptime, &dummy ) >= 1) {
				// uptime is number of seconds since boottime
				// convert to nearest time stamp
				uptime_boottime = (unsigned long)(now - uptime + 0.5);
			}
			fclose( fp );
		}

		// get stat_boottime
		if( (fp = safe_fopen_wrapper("/proc/stat", "r")) ) {
			fgets( s, 256, fp );
			while( strstr(s, "btime") == NULL ) {
				fgets( s, 256, fp );
			}
			sscanf( s, "%s %lu", junk, &stat_boottime );
			fclose( fp );
		}

		if (stat_boottime == 0 && uptime_boottime == 0 && boottime == 0) {
				// we failed to get the boottime, so we must abort
				// unless we have an old boottime value to fall back on
			dprintf( D_ALWAYS, "ProcAPI: Problem opening /proc/stat "
					 " and /proc/uptime for boottime.\n" );
			return PROCAPI_FAILURE;
		}

		if (stat_boottime != 0 || uptime_boottime != 0) {
			unsigned long old_boottime = boottime;

			// Use the older of the two boottime estimates.
			// If either one is missing, ignore it.
			if (stat_boottime == 0) boottime = uptime_boottime;
			else if (uptime_boottime == 0) boottime = stat_boottime;
			else boottime = MIN(stat_boottime,uptime_boottime);

			boottime_expiration = now+60; // update once every minute

			// Since boottime is critical for correct cpu usage
			// calculations, show how we got it.
			dprintf( D_LOAD,
					 "ProcAPI: new boottime = %lu; "
					 "old_boottime = %lu; "
					 "/proc/stat boottime = %lu; "
					 "/proc/uptime boottime = %lu\n",
					 boottime,old_boottime,stat_boottime,uptime_boottime);
		}
	}

		// success
	return PROCAPI_SUCCESS;
}

#elif defined(HPUX)

int
ProcAPI::getProcInfo( pid_t pid, piPTR& pi, int &status ) 
{

	// This *could* allocate memory and make pi point to it if pi == NULL.
	// It is up to the caller to get rid of it.
	initpi( pi );

		// get the raw system process data
	procInfoRaw procRaw;
	int retVal = ProcAPI::getProcInfoRaw(pid, procRaw, status);
	
		// if a failure occurred
	if( retVal != 0 ){
			// return failure
			// status is set by getProcInfoRaw(...)
		return PROCAPI_FAILURE;
	}

		/* clean up and convert the raw data */
	
		// if the page size hasn't been found, get it
	if( pagesize == 0 ) {	
		pagesize = getpagesize() / 1024;  // pagesize is in k now
	}
	
		// convert the memory fields from pages to k
	pi->imgsize	= procRaw.imgsize * pagesize;
	pi->rssize	= procRaw.rssize * pagesize;

		// compute the age
	pi->age = procRaw.sample_time - procRaw.creation_time;
	
		// Compute the cpu time
		// This is very inaccurrate: the below are in SECONDS!
	double cpu_time = ((double) procRaw.user_time_1 + procRaw.sys_time_1 );

		// copy the remainder of the fields
	pi->user_time	= procRaw.user_time_1;
	pi->sys_time	= procRaw.sys_time_1;
	pi->creation_time = procRaw.creation_time;
	pi->birthday    = procRaw.creation_time;
	pi->pid		= procRaw.pid;
	pi->ppid	= procRaw.ppid;
	pi->owner 	= procRaw.owner;

		/* Now that darned sampling thing, so that page faults gets
		   converted to page faults per second */

	do_usage_sampling( pi, cpu_time, procRaw.majfault, procRaw.minfault );

		// success
	return PROCAPI_SUCCESS;
}

/* Fills in procInfoRaw with the following units:
   imgsize		: pages
   rssize		: pages
   minfault		: total minor faults
   majfault		: total major faults
   user_time_1	: seconds
   user_time_2	: not set
   sys_time_1	: seconds
   sys_time_2	: not set
   creation_time: seconds since epoch
   sample_time	: seconds since epoch
*/

int
ProcAPI::getProcInfoRaw( pid_t pid, procInfoRaw& procRaw, int &status ) 
{

/* Here's getProcInfoRaw for HPUX.  Calling this a /proc interface is a lie, 
   because there IS NO /PROC for the HPUX's.  I'm using pstat_getproc().
   It returns process-specific information...pretty good, actually.  When
   called with a 3rd arg of 0 and a 4th arg of a pid, the info regarding
   that pid is returned in buf.  The bonus is, everything works...and
   you can get info on every process.
*/

		// assume success
	status = PROCAPI_OK;
	
		// clear the memory for procRaw
	initProcInfoRaw(procRaw);

		// set the sample time
	procRaw.sample_time = secsSinceEpoch();

		// get the process info
	struct pst_status buf;
	if ( pstat_getproc( &buf, sizeof(buf), 0, pid ) < 0 ) {

		// Handle "No such process" separately!
		if ( errno == ESRCH ) {
			dprintf( D_FULLDEBUG, "ProcAPI: pid %d does not exist.\n", pid );
			status = PROCAPI_NOPID;
		} else {
			status = PROCAPI_UNSPECIFIED;
			dprintf( D_ALWAYS, 
				"ProcAPI: Error in pstat_getproc(%d): %d(%s)\n",
				 pid, errno, strerror(errno) );
		}

		return PROCAPI_FAILURE;
	}

		// I have personally seen a case where the resident set size
		// was BIGGER than the image size.  However, 'top' agreed with
		// my measurements, so I guess it's just a goofy
		// bug/feature/whatever in HPUX.
	procRaw.imgsize 	= buf.pst_vdsize + buf.pst_vtsize + buf.pst_vssize;
	procRaw.rssize 		= buf.pst_rssize;
	procRaw.minfault 	= buf.pst_minorfaults;
	procRaw.majfault 	= buf.pst_majorfaults;

	procRaw.user_time_1		= buf.pst_utime;
	procRaw.user_time_2		= 0;
	procRaw.sys_time_1		= buf.pst_stime;
	procRaw.sys_time_2		= 0;
	procRaw.creation_time 	= buf.pst_start;

	procRaw.pid		= buf.pst_pid;
	procRaw.ppid	= buf.pst_ppid;
	procRaw.owner  	= buf.pst_uid;

		// success
	return PROCAPI_SUCCESS;
}

#elif defined(Darwin)

int
ProcAPI::getProcInfo( pid_t pid, piPTR& pi, int &status ) 
{

		// This *could* allocate memory and make pi point to it if pi == NULL.
		// It is up to the caller to get rid of it.
	initpi(pi);

		// get the raw system process data
	procInfoRaw procRaw;
	int retVal = ProcAPI::getProcInfoRaw(pid, procRaw, status);
	
		// if a failure occurred
	if( retVal != 0 ){
			// return failure
			// status is set by getProcInfoRaw(...)
		return PROCAPI_FAILURE;
	}

		/* Clean up and convert the raw data */

		// convert the memory fields from bytes to KB
	pi->imgsize = procRaw.imgsize / 1024;
	pi->rssize = procRaw.rssize / 1024;

		// compute the age
	pi->age = procRaw.sample_time - procRaw.creation_time;

		// compute the cpu time
	double cpu_time = procRaw.user_time_1 + procRaw.sys_time_1;

		// copy the remainder of the fields
	pi->user_time = procRaw.user_time_1;
	pi->sys_time = procRaw.sys_time_1;
	pi->creation_time = procRaw.creation_time;
	pi->birthday = procRaw.creation_time;
	pi->pid = procRaw.pid;
	pi->ppid = procRaw.ppid;
	pi->owner = procRaw.owner;
	
		// convert the number of page faults into a rate
	do_usage_sampling(pi, cpu_time, procRaw.majfault, procRaw.minfault);

	/* Byzantine kernel. :(
		It turns out that even though a non-root process
		may have the ability to call task_for_pid (either
		through configuration by the admin or default
		behavior) the kernel simply gives back very wrong
		information. This has been seen with the image size
		value and its relation with the rsssize. So for
		now, those are just plainly set to zero until a
		better solution comes along. If the process is root,
		and the kernel lied, then we'll believe it since
		this is the behavior I've seen. If sometimes this
		policy is wrong and the rootly process gets a Byzantine
		failure from the kernel, then it is simply wrong.

		This seems to only happen for ppc macosx 10.4 kernels,
		so that is where we make this policy. 

	*/
#if defined(PPC) && defined(Darwin_10_4)
	if (getuid() != 0) {
		pi->imgsize = 0;
		pi->rssize = 0;
	}
#endif

	return PROCAPI_SUCCESS;
}

/* Fills procInfoRaw with the following units:
   imgsize		: bytes
   rssize		: bytes
   minfault		: total minor faults
   majfault		: total major faults
   user_time_1	: seconds
   user_time_2	: not set
   sys_time_1	: seconds
   sys_time_2	: not set
   creation_time: seconds since epoch
   sample_time	: seconds since epoch
*/

int
ProcAPI::getProcInfoRaw( pid_t pid, procInfoRaw& procRaw, int &status ) 
{
	int mib[4];
	struct kinfo_proc *kp, *kprocbuf;
	size_t bufSize = 0;
	task_port_t task;
	task_basic_info 	ti;
	unsigned int 		count;	

	/*
		Depending upon how the user machine is configured, task_for_pid() could
		work or not work when a process is not running as root. As of macos
		10.4, the default is 0. However, since an admin may configure their
		machine to have a process utilize task_for_pid when it isn't running
		as root, we allow the admin to tell Condor about this.

		As for 09/05/2007 for macosx 10.3 & 10.4, these are the 
		applicable values.

		Old policy is:
			- the caller is running as root (EUID 0)
			- if the caller is running as the
				same UID as the target (and the target's
				EUID matches its RUID, that is, it's not
				running a setuid binary)
		New policy is:
			- if the caller was running as root (EUID 0)
			- if the pid is that of the caller
			- if the caller is running as the same UID
				as the target and the target's EUID matches
				its RUID, that is, it's not a setuid binary
				AND the caller is in group "procmod" or
				"procview"

				The long term goal of having both "procmod"
				and "procview" is that task_for_pid would
				return a send right for the task control
				port only to those processes in "procmod";
				the callers in "procview" would only get a
				send right to a task inspection port.  This
				distinction is not currently implemented.

		To actually set the policy, do this:

		$ sudo sysctl -w kern.tfp.policy=1 #set old policy
		$ sudo sysctl -w kern.tfp.policy=2 #set new policy
		$ sudo sysctl -w kern.tfp.policy=0 #disable task_for_pid except for root

		Since I can't seem to find a man page for task_for_pid() *anywhere*,
		which is odd, I'm going to declare that if it comes back not a 
		KERN_SUCCESS, then I don't have permission to see the pid. Sadly,
		I can't tell if a pid is not there, or I don't have a permission
		to see it using this method.
	*/

		// assume success
	status = PROCAPI_OK;

		// clear memory for procRaw
	initProcInfoRaw(procRaw);

		// set the sample time
	procRaw.sample_time = secsSinceEpoch();
	
		/* Collect the data from the system */
	
		// First, let's get the BSD task info for this stucture. This
		// will tell us things like the pid, ppid, etc. 
    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC;    
    mib[2] = KERN_PROC_PID;
    mib[3] = pid;
    if (sysctl(mib, 4, NULL, &bufSize, NULL, 0) < 0) {
		if (errno == ESRCH) {
			// No such process
			status = PROCAPI_NOPID;
		} else if (errno == EPERM) {
			// Operation not permitted
			status = PROCAPI_PERM;
		} else {
			status = PROCAPI_UNSPECIFIED;
		}
		dprintf( D_FULLDEBUG, 
			"ProcAPI: sysctl() (pass 1) on pid %d failed with %d(%s)\n",
			pid, errno, strerror(errno) );

        return PROCAPI_FAILURE;
    }

    kprocbuf = kp = (struct kinfo_proc *)malloc(bufSize);
	if (kp == NULL) {
		EXCEPT("ProcAPI: getProcInfo() Out of memory!\n");
	}

    if (sysctl(mib, 4, kp, &bufSize, NULL, 0) < 0) {
		if (errno == ESRCH) {
			// No such process
			status = PROCAPI_NOPID;
		} else if (errno == EPERM) {
			// Operation not permitted
			status = PROCAPI_PERM;
		} else {
			status = PROCAPI_UNSPECIFIED;
		}
		dprintf( D_FULLDEBUG, 
			"ProcAPI: sysctl() (pass 2) on pid %d failed with %d(%s)\n",
			pid, errno, strerror(errno) );

		free(kp);

        return PROCAPI_FAILURE;
    }

	// figure out the image,rss size and the sys/usr time for the process.

	kern_return_t results;
	results = task_for_pid(mach_task_self(), pid, &task);
	if(results != KERN_SUCCESS) {
		// Since we weren't able to get a mach port, we're going to assume that
		// we don't have permission to attach to the pid.  (I can't seem to
		// find a man page for this function in the vast wasteland of the
		// internet or on any machine on which I have access. So, I'm also
		// going to mash the no such process error into this case, which is
		// slightly wrong, but that should have already been caught above with
		// sysctl())--except for the race where the sysctl can succeed, then the
		// process exit, then the tfp() call happens.
		// XXX I'm sure this function call has all sorts of error edge cases I
		// am not handling due to the opaqueness of this function. :(

		// If root, then give a warning about it, but continue anyway. I've
		// seen this function fail due to the pid not being present on the
		// machine and sometimes "just because".  Also, there isn't a
		// difference when a non-root user may not call task_for_pid() and when
		// they may, but the pid isn't actually there.
		if (getuid() == 0 || geteuid() == 0) { 
			dprintf( D_FULLDEBUG, 
				"ProcAPI: task_port_pid() on pid %d failed with "
				"%d(%s), Marking imgsize, rsssize, cpu/sys time as zero for "
				"the pid.\n", pid, results, mach_error_string(results) );
		}

		// This will be set to zero at this time due to there being a LOT of
		// code change for a stable series to make it efficient when we invoke
		// /bin/ps to get this information. But if a better way to call ps is
		// created, the function call to fill this stuff in goes right here.

		procRaw.imgsize = 0;
		procRaw.rssize = 0;

		// XXX This sucks, but there is *no* method to get this which doesn't
		// involve a privledge escalation or hacky and not recommended dynamic
		// library injection in the program being tracked. Either this process
		// must be escalated or another escalated program invoked which gets
		// this for me.

		procRaw.user_time_1 = 0;
		procRaw.user_time_2 = 0;
		procRaw.sys_time_1 = 0;
		procRaw.sys_time_2 = 0;

	} else {

		/* We successfully got a mach port... */

		count = TASK_BASIC_INFO_COUNT;	
		results = task_info(task, TASK_BASIC_INFO, (task_info_t)&ti,&count);  
		if(results != KERN_SUCCESS) {
			status = PROCAPI_UNSPECIFIED;

			dprintf( D_FULLDEBUG, 
				"ProcAPI: task_info() on pid %d failed with %d(%s)\n",
				pid, results, mach_error_string(results) );

			mach_port_deallocate(mach_task_self(), task);
			free(kp);

       		return PROCAPI_FAILURE;
		}

		// fill in the values we got from the kernel
		procRaw.imgsize = (u_long)ti.virtual_size;
		procRaw.rssize = ti.resident_size;
		procRaw.user_time_1 = ti.user_time.seconds;
		procRaw.user_time_2 = 0;
		procRaw.sys_time_1 = ti.system_time.seconds;
		procRaw.sys_time_2 = 0;

		// clean up our port
		mach_port_deallocate(mach_task_self(), task);
	}


	// add in the rest
	procRaw.creation_time = kp->kp_proc.p_starttime.tv_sec;
	procRaw.pid = pid;
	procRaw.ppid = kp->kp_eproc.e_ppid;
	procRaw.owner = kp->kp_eproc.e_pcred.p_ruid; 

	// We don't know the page faults
	procRaw.majfault = 0;
	procRaw.minfault = 0;
	
	free(kp);

	// success
	return PROCAPI_SUCCESS;
}

#elif defined(CONDOR_FREEBSD)

#if defined(CONDOR_FREEBSD4)
	struct procstat {
	char comm[MAXCOMLEN+1];
	int pid;
	int ppid;
	int pgid;
	int sid;
	int tdev_maj;
	int tdev_min;
	char flags[256];
	int start;
	int start_mic;
	int utime;
	int utime_mic;
	int stime;
	int stime_mic;
	char wchan[256];
	int euid;
	int ruid;
	int rgid;
	int egid;
	char groups[256];
	};
#endif

int
ProcAPI::getProcInfo( pid_t pid, piPTR& pi, int &status ) 
{
	initpi(pi);
		//
		// get the raw system process data
		//
	procInfoRaw procRaw;
	int retVal = ProcAPI::getProcInfoRaw( pid, procRaw, status );
		//
		// If a failure occurred, then returned the right status
		//
	if ( retVal != 0 ) {
		return ( PROCAPI_FAILURE );
	}

		// if the page size has not yet been found, get it.
	if ( pagesize == 0 ) {
		pagesize = getpagesize() / 1024;
	}
	
	pi->imgsize = procRaw.imgsize / 1024;	
	pi->rssize = procRaw.rssize * getpagesize();
	pi->ppid = procRaw.ppid;
	pi->owner = procRaw.owner;
	pi->creation_time = procRaw.creation_time;
	pi->birthday = procRaw.creation_time;
	pi->pid = procRaw.pid;
	pi->user_time = procRaw.user_time_1;
	pi->sys_time = procRaw.sys_time_1;
	pi->age = secsSinceEpoch() - pi->creation_time;

	double ustime = pi->user_time + pi->sys_time;
	do_usage_sampling( pi, ustime, procRaw.majfault, procRaw.minfault );
	
	return PROCAPI_SUCCESS;
}

int
ProcAPI::getProcInfoRaw( pid_t pid, procInfoRaw& procRaw, int &status ) 
{

	int mib[4];
	struct kinfo_proc *kp, *kprocbuf;
	size_t bufSize = 0;

		//
		// Assume success
		//
	status = PROCAPI_OK;
		//
		// Clear memory for procRaw
		//
	initProcInfoRaw(procRaw);
		//
		// Set the sample time
		//
	procRaw.sample_time = secsSinceEpoch();
	
		//
		// Collect the data from the system
		// First, let's get the BSD task info for this stucture. This
		// will tell us things like the pid, ppid, etc. 
		//
	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC;
	mib[2] = KERN_PROC_PID;
	mib[3] = pid;
	if ( sysctl( mib, 4, NULL, &bufSize, NULL, 0 ) < 0 ) {
		if (errno == ESRCH) {
			// No such process
			status = PROCAPI_NOPID;
		} else if (errno == EPERM) {
			// Operation not permitted
			status = PROCAPI_PERM;
		} else {
			status = PROCAPI_UNSPECIFIED;
		}
		dprintf( D_FULLDEBUG, 
			"ProcAPI: sysctl() (pass 1) on pid %d failed with %d(%s)\n",
			pid, errno, strerror(errno) );
		return PROCAPI_FAILURE;
	}

	kprocbuf = kp = (struct kinfo_proc *)malloc(bufSize);
	if (kp == NULL) {
		EXCEPT("ProcAPI: getProcInfo() Out of memory!\n");
	}

	if (sysctl(mib, 4, kp, &bufSize, NULL, 0) < 0) {
		if (errno == ESRCH) {
			// No such process
			status = PROCAPI_NOPID;
		} else if (errno == EPERM) {
			// Operation not permitted
			status = PROCAPI_PERM;
		} else {
			status = PROCAPI_UNSPECIFIED;
		}
		dprintf( D_FULLDEBUG, 
			"ProcAPI: sysctl() (pass 2) on pid %d failed with %d(%s)\n",
			pid, errno, strerror(errno) );
		free(kp);	
		return PROCAPI_FAILURE;
	}

#if defined(CONDOR_FREEBSD4)
		// Bad! We're looking for /proc!
	FILE* fp;
	char path[MAXPATHLEN];
	struct procstat prs;
	sprintf(path,"/proc/%d/status",pid);
	if( (fp = safe_fopen_wrapper( path, "r" )) == NULL ) {
		dprintf( D_FULLDEBUG, "ProcAPI: /proc/%d/status not found!\n", pid);
		free(kp);
		return PROCAPI_FAILURE;
	}
	fscanf(fp,
		"%s %d %d %d %d %d,%d %s %d,%d %d,%d %d,%d %s %d %d %d,%d,%s",
		prs.comm, 
		&prs.pid, 
		&prs.ppid,
		&prs.pgid,
		&prs.sid, 
		&prs.tdev_maj, 
		&prs.tdev_min, 
		prs.flags, 
		&prs.start,
		&prs.start_mic,
		&prs.utime,
		&prs.utime_mic,
		&prs.stime, 
		&prs.stime_mic, 
		prs.wchan,
		&prs.euid,
		&prs.ruid,
		&prs.rgid,
		&prs.egid,
		prs.groups
	);
	struct vmspace vm = (kp->kp_eproc).e_vm; 
	procRaw.imgsize = vm.vm_map.size;
	procRaw.rssize = vm.vm_pmap.pm_stats.resident_count;
	procRaw.user_time_1 = prs.utime;
	procRaw.user_time_2 = 0;
	procRaw.sys_time_1 = prs.stime;
	procRaw.sys_time_2 = 0;
	procRaw.creation_time = prs.start;
	procRaw.pid = pid;
	procRaw.ppid = kp->kp_eproc.e_ppid;
	procRaw.owner = kp->kp_eproc.e_pcred.p_ruid; 
		// We don't know the page faults
	procRaw.majfault = 0;
	procRaw.minfault = 0;
#else
	procRaw.imgsize = kp->ki_size;
	procRaw.rssize = kp->ki_swrss;
	procRaw.ppid = kp->ki_ppid;
	procRaw.owner = kp->ki_uid;
	procRaw.majfault = kp->ki_rusage.ru_majflt;
	procRaw.minfault = kp->ki_rusage.ru_minflt;
	procRaw.user_time_1 = kp->ki_rusage.ru_utime.tv_sec;
	procRaw.user_time_2 = kp->ki_rusage.ru_utime.tv_usec;
	procRaw.sys_time_1 = kp->ki_rusage.ru_stime.tv_sec;
	procRaw.sys_time_2 = kp->ki_rusage.ru_stime.tv_usec;
	procRaw.creation_time = kp->ki_start.tv_sec;
	procRaw.pid = pid;
#endif
	free(kp);
	return PROCAPI_SUCCESS;
}
#elif defined(WIN32)

int
ProcAPI::getProcInfo( pid_t pid, piPTR& pi, int &status ) 
{

		// This *could* allocate memory and make pi point to it if pi == NULL.
		// It is up to the caller to get rid of it.
    initpi ( pi );

		// get the raw system process data
	procInfoRaw procRaw;
	int retVal = ProcAPI::getProcInfoRaw(pid, procRaw, status);
	
		// if a failure occurred
	if( retVal != 0 ){
			// return failure
			// status is set by getProcInfoRaw(...)
		return PROCAPI_FAILURE;
	}
	
		/* Clean up and convert the raw data */
	
		// convert the memory fields from bytes to KB
	pi->imgsize   = (unsigned long)(procRaw.imgsize / 1024);
    pi->rssize    = (unsigned long)(procRaw.rssize / 1024);


		// convert the time fields from the object time scale
		// to seconds
	pi->user_time 		= (long) (procRaw.user_time / procRaw.object_frequency);
    pi->sys_time  		= (long) (procRaw.sys_time / procRaw.object_frequency);

	pi->creation_time 	= (long) (procRaw.creation_time / procRaw.object_frequency - EPOCH_SHIFT);
	double cpu_time = (double)procRaw.cpu_time / procRaw.object_frequency;
	double now_secs = (double)procRaw.sample_time / procRaw.object_frequency;

		// calculate the age
	double age_wrong_scale = (double)(procRaw.sample_time - procRaw.creation_time);
	pi->age = (long)(age_wrong_scale / procRaw.object_frequency);

			// copy the remainder of the field
	pi->pid       = procRaw.pid;
    pi->ppid      = procRaw.ppid;
	pi->minfault  = 0;   // not supported by NT; all faults lumped into major.
	pi->birthday  = procRaw.creation_time;
	
		// convert fault numbers into fault rate
	do_usage_sampling ( pi, cpu_time, procRaw.majfault, procRaw.minfault, 
						now_secs );

    return PROCAPI_SUCCESS;
}

/* Fills in the procInfoRaw with the following units:
   imgsize			: bytes
   rssize			: bytes
   minfault			: not set 
   majfault			: total number of major faults
   user_time_1		: data block time scale
   user_time_2		: not set
   sys_time_1		: data block time scale
   sys_time_2		: not set
   creation_time	: data block time scale since epoch
   sample_time		: data block time scale since epoch
   object_frequency	: number of data block time scales in a second
   cpu_time			: data block time scale
*/

int
ProcAPI::getProcInfoRaw( pid_t pid, procInfoRaw& procRaw, int &status ) 
{

/* Danger....WIN32 code follows....
   The getProcInfoRaw call for WIN32 actually gets *all* the information
   on all the processes, but that's not preventable using only
   documented interfaces.  
   So, becase getProcInfo is so expensive on Win32, we cheat a little
   and try to determine if the pid is still
   around before we drudge through all this code. */

		// assume success
   status = PROCAPI_OK;

	   // clear the memory for procRaw
	initProcInfoRaw(procRaw);

	// So to first see if this pid is still alive
	// on Win32, open a handle to the pid and call GetExitStatus
	int found_it = FALSE;
	DWORD last_error;
	HANDLE pidHandle = ::OpenProcess(PROCESS_QUERY_INFORMATION,FALSE,pid);
	if (pidHandle) {
		DWORD exitstatus;
		if ( ::GetExitCodeProcess(pidHandle,&exitstatus) ) {
			if ( exitstatus == STILL_ACTIVE )
				found_it = TRUE;
		}
		::CloseHandle(pidHandle);
	} else {
		// OpenProcess() may have failed
		// due to permissions, or because all handles to that pid are gone.
		last_error = ::GetLastError();
		if ( last_error == 5 ) {
			// failure due to permissions.  this means the process object must
			// still exist, although we have no idea if the process itself still
			// does or not.  error on the safe side; return TRUE.
			found_it = TRUE;
		} else {
			// process object no longer exists, so process must be gone.
			found_it = FALSE;	
		}
	}
    if ( found_it == FALSE ) {
        dprintf( D_FULLDEBUG, 
			"ProcAPI: pid # %d was not found (OpenProcess err=%d)\n"
			,pid, last_error );
		status = PROCAPI_NOPID;
		return PROCAPI_FAILURE;
	}
	
	// pid appears to still be around, so get the stats on it
	if (GetProcessPerfData() != PROCAPI_SUCCESS) {
		status = PROCAPI_UNSPECIFIED;
		return PROCAPI_FAILURE;
	}
    
    PPERF_OBJECT_TYPE pThisObject;
    PPERF_INSTANCE_DEFINITION pThisInstance;
    long instanceNum;
    DWORD ctrblk;

    pThisObject = firstObject (pDataBlock);
  
    procRaw.sample_time = pThisObject->PerfTime.QuadPart;
    procRaw.object_frequency = pThisObject->PerfFreq.QuadPart;

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
		status = PROCAPI_NOPID;
        return PROCAPI_FAILURE;
    }

    LARGE_INTEGER elt= (LARGE_INTEGER) 
        *((LARGE_INTEGER*)(ctrblk + offsets->elapsed));
    LARGE_INTEGER pt = (LARGE_INTEGER) 
        *((LARGE_INTEGER*)(ctrblk + offsets->pctcpu));
    LARGE_INTEGER ut = (LARGE_INTEGER) 
        *((LARGE_INTEGER*)(ctrblk + offsets->utime));
    LARGE_INTEGER st = (LARGE_INTEGER) 
        *((LARGE_INTEGER*)(ctrblk + offsets->stime));
    LARGE_INTEGER imgsz = (LARGE_INTEGER) 
        *((LARGE_INTEGER*)(ctrblk + offsets->imgsize));

    procRaw.pid       = (long) *((long*)(ctrblk + offsets->procid  ));
    procRaw.ppid      = ntSysInfo.GetParentPID(pid);
    procRaw.imgsize   = imgsz.QuadPart;

	if (offsets->rssize_width == 4) {
		procRaw.rssize = *(long*)(ctrblk + offsets->rssize);
	}
	else {
		procRaw.rssize =
			((LARGE_INTEGER*)(ctrblk + offsets->rssize))->QuadPart;
	}

	procRaw.majfault  = (long) *((long*)(ctrblk + offsets->faults  ));
	procRaw.minfault  = 0;  // not supported by NT; all faults lumped into major.
	procRaw.user_time = ut.QuadPart;
    procRaw.sys_time  = st.QuadPart;

	// I do not understand why someting called "elapsed time" actually returns
	// creation time ?!?!
	procRaw.creation_time = elt.QuadPart;
	procRaw.cpu_time	=  pt.QuadPart;

		// success
    return PROCAPI_SUCCESS;
}

// the Windows version of buildProcInfos(); since we always pull information about
// all processes out of HKEY_PERFORMANCE_DATA, this is not much different from
// what happens above in getProcInfoRaw
//
int
ProcAPI::buildProcInfoList()
{
	deallocAllProcInfos();

	if (GetProcessPerfData() != PROCAPI_SUCCESS) {
		return PROCAPI_FAILURE;
	}

	// If we haven't yet gotten the offsets, grab 'em.
	//
	PPERF_OBJECT_TYPE pThisObject = firstObject(pDataBlock);
    if( !offsets ) {
        grabOffsets( pThisObject );
	}
	PPERF_INSTANCE_DEFINITION pThisInstance = firstInstance(pThisObject);

	__int64 sampleObjectTime = pThisObject->PerfTime.QuadPart;
	__int64 objectFrequency  = pThisObject->PerfFreq.QuadPart;

    // loop through each instance in data
	//
	DWORD ctrblk;
    int instanceNum = 0;
    while( instanceNum < pThisObject->NumInstances ) {

		procInfo* pi;

        ctrblk = ((DWORD)pThisInstance) + pThisInstance->ByteLength;

		LARGE_INTEGER elt = *(LARGE_INTEGER*)(ctrblk + offsets->elapsed);
        LARGE_INTEGER pt = *(LARGE_INTEGER*)(ctrblk + offsets->pctcpu);
        LARGE_INTEGER ut = *(LARGE_INTEGER*)(ctrblk + offsets->utime);
        LARGE_INTEGER st = *(LARGE_INTEGER*)(ctrblk + offsets->stime);

		pi = NULL;
		initpi(pi);

        pi->pid = *(pid_t*)(ctrblk + offsets->procid);
		pi->ppid = ntSysInfo.GetParentPID(pi->pid);

		LARGE_INTEGER* liptr;
		liptr = (LARGE_INTEGER*)(ctrblk + offsets->imgsize);
		pi->imgsize = (unsigned long)(liptr->QuadPart / 1024);
		if (offsets->rssize_width == 4) {
			pi->rssize = *(long*)(ctrblk + offsets->rssize) / 1024;
		}
		else {
			liptr = (LARGE_INTEGER*)(ctrblk + offsets->rssize);
			pi->rssize = (unsigned long)(liptr->QuadPart / 1024);
		}

		pi->user_time = (long)(ut.QuadPart / objectFrequency);
		pi->sys_time = (long) (st.QuadPart / objectFrequency);

		// I do not understand why someting called "elapsed time" actually returns
		// creation time ?!?!
		pi->birthday = elt.QuadPart;
		pi->creation_time 	= (long)(pi->birthday / objectFrequency - EPOCH_SHIFT);

		pi->age = (long)((sampleObjectTime - pi->birthday) / objectFrequency);

                        /* We figure out the cpu usage (a total counter, not a
                           percent!) and the total page faults here. */
		double cpu = LI_to_double( pt ) / objectFrequency;
		long faults = *(long*)(ctrblk + offsets->faults);

		/* figure out the %cpu and %faults */
		do_usage_sampling (pi, cpu, faults, 0, sampleObjectTime / objectFrequency);

		pi->next = allProcInfos;
		allProcInfos = pi;

		pThisInstance = nextInstance( pThisInstance );
		instanceNum++;
	}

	return PROCAPI_SUCCESS;
}

int
ProcAPI::GetProcessPerfData()
{
	DWORD dwStatus;  // return status of fn. calls

        // '2' is the 'system' , '230' is 'process'  
        // I hope these numbers don't change over time... 
    dwStatus = GetSystemPerfData ( TEXT("230") );

    if( dwStatus != ERROR_SUCCESS ) {
        dprintf( D_ALWAYS,
		         "ProcAPI: failed to get process data from registry\n");
        return PROCAPI_FAILURE;
    }

        // somehow we don't have the process data -> panic
    if( pDataBlock == NULL ) {
        dprintf( D_ALWAYS,
		         "ProcAPI: failed to make pDataBlock.\n");
        return PROCAPI_FAILURE;
    }

	return PROCAPI_SUCCESS;
}

#elif defined(AIX)

int
ProcAPI::getProcInfo( pid_t pid, piPTR& pi, int &status ) 
{

		// This *could* allocate memory and make pi point to it if 
		// pi == NULL. It is up to the caller to get rid of it.
	initpi( pi );

		// get the raw system process data
	procInfoRaw procRaw;
	int retVal = ProcAPI::getProcInfoRaw(pid, procRaw, status);
	
		// if a failure occurred
	if( retVal != 0 ){
			// return failure
			// status is set by getProcInfoRaw(...)
		return PROCAPI_FAILURE;
	}

		/* Clean up and convert the raw data */
	
		// Convert the image size from pages to KB
	pi->imgsize = procRaw.imgsize * getpagesize() / 1024;


		// Calculate the age
	pi->age = procRaw.sample_time - procRaw.creation_time;

			// Compute cpu usage
	pi->cpuusage = 0.0; /* XXX fixme compute it */
		
		// Copy the remainder of the fields
	pi->rssize = procRaw.rssize; /* XXX not really right */
	pi->minfault = procRaw.minfault;
	pi->majfault = procRaw.majfault;
	pi->user_time = procRaw.user_time_1; /* XXX fixme microseconds */
	pi->sys_time = procRaw.sys_time_1; /* XXX fixme microseconds */
	pi->pid = procRaw.pid;
	pi->ppid = procRaw.ppid;
	pi->creation_time = procRaw.creation_time;
	pi->birthday = procRaw.creation_time;
	pi->owner = procRaw.owner;

		// success
	return PROCAPI_SUCCESS;
	
}

/* Fills in the procInfoRaw with the following units:
   imgsize		: pages
   rssize		: don't know
   minfault		: total number of minor faults
   majfault		: total number of major faults
   user_time_1	: microseconds (I think)
   user_time_2	: not set
   sys_time_1	: microseconds (I think)
   sys_time_2	: not set
   creation_time: seconds since epoch
   sample_time	: seconds since epoch
 */
int
ProcAPI::getProcInfoRaw( pid_t pid, procInfoRaw& procRaw, int &status ) 
{
	struct procentry64 pent;
	struct fdsinfo64 fent;
	int retval;

		// assume success
	status = PROCAPI_OK;

		// clear the memory for procRaw
	initProcInfoRaw(procRaw);

		// set the sample time
	procRaw.sample_time = secsSinceEpoch();

	/* I do this so the getprocs64() call doesn't affect what we passed in */

	/* Ask the OS for this one process entry, based on the pid */
	pid_t index = 0;

	/* check every single pid until getprocs64 returns something less than
		count, the terminating condition, or we find the pid.
		This function is probably expensive and maybe more should be asked
		for instead of one pid at a time.... */
	while( 1 )
	{
		retval = getprocs64(&pent, sizeof(struct procentry64),
							&fent, sizeof(struct fdsinfo64),
							&index, 1);

		if ( retval == -1 )
		{
			// some odd problem with getprocs64(), let the caller figure it out
			status = PROCAPI_UNSPECIFIED;
			return PROCAPI_FAILURE;

			// Not found.  Ug.
		} else if ( retval == 0 ) {
			status = PROCAPI_NOPID;
			return PROCAPI_FAILURE;

			// Found our match?
		} else if ( pid == pent.pi_pid ) {
			procRaw.imgsize = pent.pi_size;
			procRaw.rssize = pent.pi_drss + pent.pi_trss; /* XXX not really right */
			procRaw.minfault = pent.pi_minflt;
			procRaw.majfault = pent.pi_majflt;
			procRaw.user_time_1 = pent.pi_ru.ru_utime.tv_usec; /* XXX fixme microseconds */
			procRaw.user_time_2 = 0;
			procRaw.sys_time_1 = pent.pi_ru.ru_stime.tv_usec; /* XXX fixme microseconds */
			procRaw.sys_time_2 = 0;
			procRaw.pid = pent.pi_pid;
			procRaw.ppid = pent.pi_ppid;
			procRaw.creation_time = pent.pi_start;
			procRaw.owner = pent.pi_uid;

			// All done
			return PROCAPI_SUCCESS;
		}
	}
	status = PROCAPI_UNSPECIFIED;
	return PROCAPI_FAILURE;
}

#else
#error Please define ProcAPI::getProcInfo( pid_t pid, piPTR& pi, int &status ) for this platform!
#endif


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
        dprintf ( D_ALWAYS, "ProcAPI sanity failure on pid %d, cpuusage = %f\n", 
                  pi->pid, pi->cpuusage );
        pi->cpuusage = 0.0;
	}
	if( pi->user_time < 0 ) {
		dprintf ( D_ALWAYS, "ProcAPI sanity failure on pid %d, user_time = %ld\n", 
				  pi->pid, pi->user_time );
		pi->user_time = 0;
	}
	if( pi->sys_time < 0 ) {
		dprintf ( D_ALWAYS, "ProcAPI sanity failure on pid %d, sys_time = %ld\n", 
				  pi->pid, pi->sys_time );
		pi->sys_time = 0;
	}
	if( pi->age < 0 ) {
		dprintf ( D_ALWAYS, "ProcAPI sanity failure on pid %d, age = %ld\n", 
				  pi->pid, pi->age );
		pi->age = 0;
	}

		// now we can delete this.
	if ( phn ) delete phn;
	
}

procInfo*
ProcAPI::getProcInfoList()
{
#if !defined(WIN32) && !defined(HPUX) && !defined(AIX)
	buildPidList();
#endif

	if (buildProcInfoList() != PROCAPI_SUCCESS) {
		dprintf(D_ALWAYS,
		        "ProcAPI: error retrieving list of process data\n");
		deallocAllProcInfos();
	}

#if !defined(WIN32) && !defined(HPUX) && !defined(AIX)
	deallocPidList();
#endif

	procInfo* ret = allProcInfos;
	allProcInfos = NULL;

	return ret;
}

void
ProcAPI::freeProcInfoList(procInfo* pi)
{
	if( pi != NULL ) {
		piPTR prev;
		piPTR temp = pi;
		while( temp != NULL ) {
			prev = temp;
			temp = temp->next;
			delete prev;
		}
	}
}

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
#if !defined(WIN32)
	pi->owner    = 0;
#endif

	pidenvid_init(&pi->penvid);
}

void
ProcAPI::initProcInfoRaw(procInfoRaw& procRaw){
	memset(&procRaw, 0, sizeof(procInfoRaw));
}

#ifndef WIN32

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

/* Wonderfully enough, this works for all OS'es except OS X and HPUX. 
   OS X has it's own version, HP-UX never calls it.
   This function opens
   up the /proc directory and reads the pids of all the processes in the
   system.  This information is put into a list of pids that is pointed
   to by pidList, a private data member of ProcAPI.  
 */

#if !defined(Darwin) && !defined(CONDOR_FREEBSD)
int
ProcAPI::buildPidList() {

	condor_DIR *dirp;
	pidlistPTR current;
	pidlistPTR temp;

		// make a header node for the pidList:
	deallocPidList();
	pidList = new pidlist;

	current = pidList;

	dirp = condor_opendir("/proc");
	if( dirp != NULL ) {
			// NOTE: this will use readdir64() when available to avoid
			// skipping over directories with an inode value that
			// doesn't happen to fit in the 32-bit ino_t
		condor_dirent *direntp;
		while( (direntp = condor_readdir(dirp)) != NULL ) {
			if( isdigit(direntp->d_name[0]) ) {   // check for first char digit
				temp = new pidlist;
				temp->pid = (pid_t) atol ( direntp->d_name );
				temp->next = NULL;
				current->next = temp;
				current = temp;
			}
		}
		condor_closedir( dirp );
    
		temp = pidList;
		pidList = pidList->next;
		delete temp;           // remove header node.

		return PROCAPI_SUCCESS;
	} 

	delete pidList;        // remove header node.
	pidList = NULL;

	return PROCAPI_FAILURE;
}
#endif
/* 
   The darwin/OS X version of this code - it should work just fine on 
   FreeBSD as well, but FreeBSD does have a /proc that it could look at
 */

#ifdef Darwin
int
ProcAPI::buildPidList() {

	pidlistPTR current;
	pidlistPTR temp;

		// make a header node for the pidList:
	deallocPidList();
	pidList = new pidlist;

	current = pidList;

	int mib[4];
	struct kinfo_proc *kp, *kprocbuf;
	size_t origBufSize;
	size_t bufSize = 0;
	int nentries;

	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC;
	mib[2] = KERN_PROC_ALL;
	mib[3] = 0;

		//
		// Returns back the size of the kinfo_proc struct
		//
	if (sysctl(mib, 4, NULL, &bufSize, NULL, 0) < 0) {
		 //perror("Failure calling sysctl");
		return PROCAPI_FAILURE;
	}	

	kprocbuf = kp = (struct kinfo_proc *)malloc(bufSize);

	origBufSize = bufSize;
	if ( sysctl(mib, 4, kp, &bufSize, NULL, 0) < 0) {
		free(kprocbuf);
		return PROCAPI_FAILURE;
	}

	nentries = bufSize / sizeof(struct kinfo_proc);

	for(int i = nentries; --i >=0; kp++) {
		temp = new pidlist;
		temp->pid = (pid_t) kp->kp_proc.p_pid;
		temp->next = NULL;
		current->next = temp;
		current = temp;
	}
    
	temp = pidList;
	pidList = pidList->next;
	delete temp;           // remove header node.

	free(kprocbuf);

	return PROCAPI_SUCCESS;
}

#endif

#if defined(CONDOR_FREEBSD)
int
ProcAPI::buildPidList() {

	pidlistPTR current;
	pidlistPTR temp;

		// make a header node for the pidList:
	deallocPidList();
	pidList = new pidlist;

	current = pidList;

	int mib[4];
	struct kinfo_proc *kp, *kprocbuf;
	size_t origBufSize;
	size_t bufSize = 0;
	int nentries;

	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC;
	mib[2] = KERN_PROC_ALL;
	mib[3] = 0;
	if (sysctl(mib, 3, NULL, &bufSize, NULL, 0) < 0) {
		 //perror("Failure calling sysctl");
		return PROCAPI_FAILURE;
	}	

	kprocbuf = kp = (struct kinfo_proc *)malloc(bufSize);

	origBufSize = bufSize;
	if ( sysctl(mib, 3, kp, &bufSize, NULL, 0) < 0) {
		free(kprocbuf);
		return PROCAPI_FAILURE;
	}

	nentries = bufSize / sizeof(struct kinfo_proc);

	for(int i = nentries; --i >=0; kp++) {
		temp = new pidlist;
#if defined(CONDOR_FREEBSD4)
		temp->pid = (pid_t) kp->kp_proc.p_pid;
#else
		temp->pid = kp->ki_pid;
#endif
		temp->next = NULL;
		current->next = temp;
		current = temp;
	}
    
	temp = pidList;
	pidList = pidList->next;
	delete temp;           // remove header node.

	free(kprocbuf);

	return PROCAPI_SUCCESS;
}
#endif

#ifdef HPUX
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

			temp->imgsize       = ( pst[i].pst_vdsize + pst[i].pst_vtsize 
			                      + pst[i].pst_vssize )   * pagesize;
			temp->rssize        = pst[i].pst_rssize * pagesize;
			temp->minfault      = pst[i].pst_minorfaults;
			temp->majfault      = pst[i].pst_majorfaults;
			temp->user_time     = pst[i].pst_utime;
			temp->sys_time      = pst[i].pst_stime;
			temp->creation_time = pst[i].pst_start;
			temp->birthday      = pst[i].pst_start;
			temp->age           = secsSinceEpoch() - pst[i].pst_start;
			temp->cpuusage      = pst[i].pst_pctcpu * 100;
			temp->pid           = pst[i].pst_pid;
			temp->ppid          = pst[i].pst_ppid;
			temp->next          = NULL;
			
			// save the newly created node into the list
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
		return PROCAPI_FAILURE;
	}

	return PROCAPI_SUCCESS;
}
// end of HPUX's buildProcInfoList()

#elif defined AIX
/* now for the AIX version.  In a sense, it's simpler, because no
   pidlist has to be built......the allProcInfos list
   is built by repeated calls to getprocs64(), which will return
   information about every process in the system, if asked enough
   times and in the proper manner.
*/
int
ProcAPI::buildProcInfoList() {
  
		// this determines the number of process infos to grab at
		// once.   10 seems to be a good number.
	const int BURSTSIZE = 10;    
	piPTR current;

		// make a header node for ease of list construction:
	deallocAllProcInfos();
	allProcInfos = new procInfo;
	current = allProcInfos;
	current->next = NULL;

		// loop until count == 0, which will occur when all have been returned
	pid_t idx = 0;  // index within the context
	while( 1 ) {
		struct procentry64 pent[ BURSTSIZE ];
		struct fdsinfo64 fent[ BURSTSIZE ];

		// Ask AIX for a group of processes
		int count = getprocs64( pent, sizeof(struct procentry64),
								fent, sizeof(struct fdsinfo64),
								&idx, BURSTSIZE );

		// some odd problem with getprocs64(), let the caller figure it out
		if ( count < 0 ) {
			dprintf( D_ALWAYS, "ProcAPI: getprocs64() failed\n" );
			return PROCAPI_FAILURE;
		} else if ( count == 0 ) {
			break;
		}

		// Loop through & add them all
		for( int i=0;  i<count;  i++ ) {
			piPTR pi = new procInfo;

			// This *could* allocate memory and make pi point to it if 
			// pi == NULL. It is up to the caller to get rid of it.
			initpi( pi );

			pi->imgsize = pent[i].pi_size * getpagesize() / 1024;
			pi->rssize = pent[i].pi_drss + pent[i].pi_trss; /* XXX not really right */
			pi->minfault = pent[i].pi_minflt;
			pi->majfault = pent[i].pi_majflt;
			pi->user_time = pent[i].pi_ru.ru_utime.tv_usec; /* XXX fixme microseconds */
			pi->sys_time = pent[i].pi_ru.ru_stime.tv_usec; /* XXX fixme microseconds */
			pi->age = secsSinceEpoch() - pent[i].pi_start;
			pi->pid = pent[i].pi_pid;
			pi->ppid = pent[i].pi_ppid;
			pi->creation_time = pent[i].pi_start;
			pi->birthday = pent[i].pi_start;

			pi->cpuusage = 0.0; /* XXX fixme compute it */
			
			// save the newly created node into the list
			current->next = pi;
			current = pi;
		}
	}

	// remove that header node:
	piPTR temp = allProcInfos;
	allProcInfos = allProcInfos->next;
	delete temp;

	return PROCAPI_SUCCESS;
}

/* Using the list of processes pointed to by pidList and returned by
   getAndRemNextPid(), a linked list of procInfo structures is
   built.  At the end, allProcInfos will point to the head of this
   list.  This function is used on all OS's except for HPUX or AIX.
*/
#else
int
ProcAPI::buildProcInfoList() {
  
	piPTR current;
	piPTR temp;
	pid_t thispid;
	int status;

		// make a header node for ease of list construction:
	deallocAllProcInfos();
	allProcInfos = new procInfo;
	current = allProcInfos;
	current->next = NULL;

	temp = NULL;
	while( (thispid = getAndRemNextPid()) >= 0 ) {
		if( getProcInfo(thispid, temp, status) == PROCAPI_SUCCESS) {
			current->next = temp;
			current = temp;
			temp = NULL;
		}
		else if (temp != NULL) {
			delete temp;
			temp = NULL;
		}
	}

		// we're done; remove header node.
	temp = allProcInfos;
	allProcInfos = allProcInfos->next;
	delete temp;

	return PROCAPI_SUCCESS;
}
#endif

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

void
ProcAPI::printProcInfo( piPTR pi ) {
	printProcInfo(stdout, pi);
}

void
ProcAPI::printProcInfo(FILE* fp, piPTR pi){
	if( pi == NULL ) {
		return;
	}
	fprintf( fp, "process image, rss, in k: %lu, %lu\n", pi->imgsize, pi->rssize );
	fprintf( fp, "minor & major page faults: %lu, %lu\n", pi->minfault, 
			pi->majfault ); 
	fprintf( fp, "Times:  user, system, creation, age: %ld %ld %ld %ld\n", 
			pi->user_time, pi->sys_time, pi->creation_time, pi->age );
	fprintf( fp, "percent cpu usage of this process: %5.2f\n", pi->cpuusage);
	fprintf( fp, "pid is %d, ppid is %d\n", pi->pid, pi->ppid);
	fprintf( fp, "\n" );
}

#ifndef WIN32

uid_t 
ProcAPI::getFileOwner(int fd) {
	
#if HAVE_FSTAT64
	// If we do not use fstat64(), fstat() fails if the inode number
	// is too big and possibly for a few other reasons as well.
	struct stat64 si;
	if ( fstat64(fd, &si) != 0 ) {
#else
	struct stat si;
	if ( fstat(fd, &si) != 0 ) {
#endif
		dprintf(D_ALWAYS, 
			"ProcAPI: fstat failed in /proc! (errno=%d)\n", errno);
		return 0; 	// 0 is probably wrong, but this should never
					// happen (unless things are really screwed)
	}

	return si.st_uid;
}


 
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

#endif // not defined WIN32

void 
ProcAPI::deallocAllProcInfos() {

	freeProcInfoList(allProcInfos);
	allProcInfos = NULL;
}

unsigned int
pidHashFunc( const pid_t& pid ) {
	return pid;   
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

static void
unexpected_counter_size(const char* counter, int actual, const char* expected)
{
	EXCEPT("Unexpected performance counter size for %s: %d (expected %s); "
	           "Registry key HKEY_LOCAL_MACHINE\\SYSTEM\\"
	           "CurrentControlSet\\Services\\PerfProc\\Performance must "
	           "have 'Disable Performance Counters' value of 0 or no "
	           "such value",
	       counter,
	       actual,
	       expected);
}

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
	if (pThisCounter->CounterSize != 8) {
		unexpected_counter_size("total CPU", pThisCounter->CounterSize, "8");
	}
    
    pThisCounter = nextCounter(pThisCounter);
//    printcounter ( stdout, pThisCounter );
    offsets->utime = pThisCounter->CounterOffset;  // % user time
	if (pThisCounter->CounterSize != 8) {
		unexpected_counter_size("user CPU", pThisCounter->CounterSize, "8");
	}
  
    pThisCounter = nextCounter(pThisCounter);
//    printcounter ( stdout, pThisCounter );
    offsets->stime = pThisCounter->CounterOffset;  // % sys time
	if (pThisCounter->CounterSize != 8) {
		unexpected_counter_size("system CPU", pThisCounter->CounterSize, "8");
	}
  
    pThisCounter = nextCounter(pThisCounter);
    pThisCounter = nextCounter(pThisCounter);
//    printcounter ( stdout, pThisCounter );
    offsets->imgsize = pThisCounter->CounterOffset;  // image size
	if (pThisCounter->CounterSize != 8) {
		unexpected_counter_size("image size", pThisCounter->CounterSize, "8");
	}
  
    pThisCounter = nextCounter(pThisCounter);
//    printcounter ( stdout, pThisCounter );
    offsets->faults = pThisCounter->CounterOffset;   // page faults
	if (pThisCounter->CounterSize != 4) {
		unexpected_counter_size("page faults", pThisCounter->CounterSize, "4");
	}
    
    pThisCounter = nextCounter(pThisCounter);
    offsets->rssize = pThisCounter->CounterOffset;   // working set peak 
	offsets->rssize_width = pThisCounter->CounterSize;
	if ((offsets->rssize_width != 4) && (offsets->rssize_width != 8)) {
		unexpected_counter_size("working set",
		                        pThisCounter->CounterSize,
		                        "4 or 8");
	}

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
	if (pThisCounter->CounterSize != 8) {
		unexpected_counter_size("elapsed time",
		                        pThisCounter->CounterSize,
		                        "8");
	}
    
    pThisCounter = nextCounter(pThisCounter);
//    printcounter ( stdout, pThisCounter );
    offsets->procid = pThisCounter->CounterOffset;   // process id
		if (pThisCounter->CounterSize != 4) {
		unexpected_counter_size("PID", pThisCounter->CounterSize, "4");
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

int
ProcAPI::createProcessId(pid_t pid, ProcessId*& pProcId, int& status, int* precision_range){

		// assume success
	status = PROCAPI_OK;
	
		// get the control time
	long ctlTimeA = 0;
	if( generateControlTime(ctlTimeA, status) == PROCAPI_FAILURE ){
			// return failure
			// status is set by generateControlTime(...)
		return PROCAPI_FAILURE;
	}
	
		/* Continue getting the process information
		   until the control time stabalizes. */
	long ctlTimeB = ctlTimeA;
	procInfoRaw procRaw;
	int tries = 0;
	do{
			// use the last ctl time
		ctlTimeA = ctlTimeB;

		
		// get the raw system process data
		if( ProcAPI::getProcInfoRaw(pid, procRaw, status) == PROCAPI_FAILURE ){			
				// a failure occurred
				// status is set by getProcInfoRaw(...)
			return PROCAPI_FAILURE;
		}

			// get the control time again
		if( generateControlTime(ctlTimeB, status) == PROCAPI_FAILURE ){
				// a failure occurred
				// status is set by generateControlTime(...)
			return PROCAPI_FAILURE;
		}
		
		tries++;
	} while( ctlTimeA != ctlTimeB && tries < ProcAPI::MAX_SAMPLES);

		// Failed
	if( ctlTimeA != ctlTimeB ){
		status = PROCAPI_UNSPECIFIED;
		dprintf(D_ALWAYS,
				"ProcAPI: Control time was too unstable to generate a signature for pid: %d\n",
				pid);
		return PROCAPI_FAILURE;
	}

		// Determine the precision
	if( precision_range == NULL ){
		precision_range = &DEFAULT_PRECISION_RANGE;
	}
	
		// convert to the same time units as the rest of 
		// the process id
	*precision_range = (int)ceil( ((double)*precision_range) * TIME_UNITS_PER_SEC);

		/* Initialize the Process Id
		   This WILL ALWAYS create memory the caller is responsible for.
		*/
	pProcId = new ProcessId(pid, procRaw.ppid, 
							*precision_range,
							TIME_UNITS_PER_SEC,
							procRaw.creation_time, ctlTimeA);

		// success
	return PROCAPI_SUCCESS;
		
}

int
ProcAPI::isAlive(const ProcessId& procId, int& status){

		// assume success
	status = PROCAPI_OK;


		// get the current id associated with this pid
	ProcessId* pNewProcId = NULL;
	int retVal = createProcessId(procId.getPid(), pNewProcId, status);
	
		// error getting the process id
	if( retVal == PROCAPI_FAILURE && status != PROCAPI_NOPID ){
			// status is set by createProcessId(..)
		return PROCAPI_FAILURE;
	}
	
		// no matching pid, process is dead
	else if( retVal == PROCAPI_FAILURE && status == PROCAPI_NOPID ){
			// set status to dead
		status = PROCAPI_DEAD;
			// early success
		return PROCAPI_SUCCESS;;
	}

		// see if the processes are the same
	retVal = procId.isSameProcess(*pNewProcId);

		// the processes are the same
	switch(retVal) {
		case ProcessId::SAME:
			status = PROCAPI_ALIVE;
			break;

		case ProcessId::DIFFERENT:
			status = PROCAPI_DEAD;
			break;

		case ProcessId::UNCERTAIN:
			status = PROCAPI_UNCERTAIN;
			break;

		default:
			status = PROCAPI_UNSPECIFIED;
			dprintf(D_ALWAYS,
					"ProcAPI: ProcessId::isSameProcess(..) returned an "
					"unexpected value for pid: %d\n", procId.getPid());
			delete pNewProcId;
			return PROCAPI_FAILURE;
			break;
	}
	
		// clean up
	delete pNewProcId;

		// success
	return PROCAPI_SUCCESS;
}

#ifdef LINUX
/*
  Currently only Linux has the capability of confirming a process.
  So it is the only system that has this function defined to
  do anything useful.

  If you think another OS should be able to confirm processes it must
  have one of the following two properties: 
  1. Process birthdays remain the same after the time has been reset 
  by the admin or ntpd.
  2. Process birthdays may change due to time changes, but there is 
  a control time that changes along with the birthdays that can be used
  to reconstruct the old birthday.  Specifically:
  new_bday = (new_ctl_time - old_ctl_time) + old_bday.
  Further, the control time must be in the same units as the birthday 
  and the confirm time.

  If confirm is going to be defined to do something useful for your OS,
  generateConfirmTime(...) must also be usefully defined.  It should
  generate the current time in the same units and time space as the 
  birthday.  For example, since birthdays in Linux are in jiffies since
  boot, the confirm time is uptime (also since boot, hence same timespace)
  converted to jiffies.

  - Joe Meehean 12/12/05
*/
int
ProcAPI::confirmProcessId(ProcessId& procId, int& status){
	
		// assume success
	status = PROCAPI_OK;

		// get the control time
	long ctlTimeA = 0;
	if( generateControlTime(ctlTimeA, status) == PROCAPI_FAILURE ){
		// status is set by generateControlTime(...)
		return PROCAPI_FAILURE;
	}
	
	/* Continue getting the confirmation time
	   until the control time stabalizes. */
	long ctlTimeB = ctlTimeA;
	long confirmation_time = 0;
	int tries = 0;
	do{
			// use the last ctl time
		ctlTimeA = ctlTimeB;

			// get the confirm time
		if( generateConfirmTime(confirmation_time, status) == PROCAPI_FAILURE ){
				// status is set by generateConfirmTime(...)
			return PROCAPI_FAILURE;
		}

			// get the control time again
		if( generateControlTime(ctlTimeB, status) == PROCAPI_FAILURE ){
				// status is set by generateControlTime(...)
			return PROCAPI_FAILURE;
		}

		tries++;
	} while( ctlTimeA != ctlTimeB && tries < MAX_SAMPLES );

		// Failed
	if( ctlTimeA != ctlTimeB ){
		status = PROCAPI_UNSPECIFIED;
		dprintf(D_ALWAYS,
				"ProcAPI: Control time was too unstable to generate a confirmation for pid: %d\n",
				procId.getPid());
		return PROCAPI_FAILURE;
	}
	
		// confirm the process id
	if( procId.confirm(confirmation_time, ctlTimeA) == ProcessId::FAILURE ){
		status = PROCAPI_UNSPECIFIED;
		dprintf(D_ALWAYS,
				"ProcAPI: Could not confirm process for pid: %d\n",
				procId.getPid());
		return PROCAPI_FAILURE;
	}
		//success
	return PROCAPI_SUCCESS;
}


int
ProcAPI::generateControlTime(long& ctl_time, int& status){
	
		// birthdays don't change when time shifts
	ctl_time = 0;
	
		// success
	status = PROCAPI_OK;
	return PROCAPI_SUCCESS;
}


// uptime on Linux converted to jiffies
int
ProcAPI::generateConfirmTime(long& confirm_time, int& status){

		// open the uptime file
	FILE* fp = safe_fopen_wrapper("/proc/uptime", "r");
	if( fp == NULL ) {
		dprintf(D_ALWAYS, "Failed to open /proc/uptime: %s\n", 
				strerror(errno)
				);
		status = PROCAPI_UNSPECIFIED;
		return PROCAPI_FAILURE;
	}
	
		// scan it for the uptime (first field)
	double uptime=0;
	double junk=0;
	if (fscanf( fp, "%lf %lf", &uptime, &junk ) < 1) {
		dprintf(D_ALWAYS, "Failed to get uptime from /proc/uptime\n"); 
		status = PROCAPI_UNSPECIFIED;
		return PROCAPI_FAILURE;
	}

		// close the file
	fclose( fp );

		// convert uptime into jiffies
		/* May roll over if the machine has been up for longer than 248 days.
		   Luckily this is about how often a new flavor of Linux is released.
		*/
	confirm_time = (long)(uptime * 100);

		// success
	status = PROCAPI_OK;
	return PROCAPI_SUCCESS;
}

#else // everything else

int
ProcAPI::confirmProcessId(ProcessId& procId, int& status){
		// do nothing
	status = PROCAPI_OK;
	return PROCAPI_SUCCESS;
}

int
ProcAPI::generateControlTime(long& ctl_time, int& status){
		// non Linux machines currently don't handle time shifts
	ctl_time = 0;
	
		// success
	status = PROCAPI_OK;
	return PROCAPI_SUCCESS;
}

int
ProcAPI::generateConfirmTime(long& confirm_time, int& status){
		// non Linux machines currently can't confirm
		// process ids
	confirm_time = 0;
	
		// success
	status = PROCAPI_OK;
	return PROCAPI_SUCCESS;
}

#endif // not linux
