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
long ProcAPI::boottime_expiration = 0;
#endif // LINUX
#else // WIN32
#include "ntsysinfo.h"
static CSysinfo ntSysInfo;	// for getting parent pid on NT
PPERF_DATA_BLOCK ProcAPI::pDataBlock	= NULL;
ExtArray<HANDLE> ProcAPI::familyHandles;
struct Offset * ProcAPI::offsets		= NULL;
#endif // WIN32
int ProcAPI::MAX_SAMPLES = 5;
int ProcAPI::DEFAULT_PRECISION_RANGE = 4;

#ifdef LINUX
double ProcAPI::TIME_UNITS_PER_SEC = (double)HZ;
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
    deallocAllProcInfos();
    deallocProcFamily();
#endif

    struct procHashNode * phn;
    procHash->startIterations();
    while ( procHash->iterate( phn ) )
        delete phn;
    
    delete procHash;

	closeFamilyHandles();
    
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

		// up our privledges
	priv_state priv = set_root_priv();

	
		// open /proc/<pid>
	sprintf( path, "/proc/%d", pid );
	if( (fd = open(path, O_RDONLY)) < 0 ) {
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

			// reset our privledges
		set_priv( priv );
		return PROCAPI_FAILURE;
	}


	// PIOCPSINFO gets memory sizes, pids, and age.
	if ( ioctl( fd, PIOCPSINFO, &pri ) < 0 ) {
		dprintf( D_ALWAYS, "ProcAPI: PIOCPSINFO Error occurred for pid %d\n",
				 pid );

		close( fd );

			// reset our privledges
		set_priv( priv );

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
		
			// reset our privledges
		set_priv( priv );

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

			// reset our privledges
		set_priv( priv );

		status = PROCAPI_UNSPECIFIED;
		return PROCAPI_FAILURE;
	}

	procRaw.user_time_1 = prs.pr_utime.tv_sec;
	procRaw.user_time_2 = prs.pr_utime.tv_nsec;
	procRaw.sys_time_1 = prs.pr_stime.tv_sec;
	procRaw.sys_time_2 = prs.pr_stime.tv_nsec;

	// close the /proc/pid file
	close ( fd );

		// reset our privledges
	set_priv( priv );

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

		// up our privledges
	priv_state priv = set_root_priv();


		// pids, memory usage, and age can be found in 'psinfo':
	sprintf( path, "/proc/%d/psinfo", pid );
	if( (fd = open(path, O_RDONLY)) < 0 ) {

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

		set_priv( priv );

		return PROCAPI_FAILURE;
	} 

	// grab the information from the file descriptor.
	if( read(fd, &psinfo, sizeof(psinfo_t)) != sizeof(psinfo_t) ) {
		dprintf( D_ALWAYS, 
			"ProcAPI: Unexpected short read while reading %s.\n", path );

		status = PROCAPI_GARBLED;

		set_priv( priv );

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
	if( (fd = open(path, O_RDONLY) ) < 0 ) {

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

		set_priv( priv );

		return PROCAPI_FAILURE;

	}

	if( read(fd, &prusage, sizeof(prusage_t)) != sizeof(prusage_t) ) {

		dprintf( D_ALWAYS, 
			"ProcAPI: Unexpected short read while reading %s.\n", path );

		status = PROCAPI_GARBLED;

		set_priv( priv );

		return PROCAPI_FAILURE;
	}

	close( fd );

	procRaw.minfault = prusage.pr_minf;
	procRaw.majfault = prusage.pr_majf;
	procRaw.user_time_1 = prusage.pr_utime.tv_sec;
	procRaw.user_time_2 = prusage.pr_utime.tv_nsec;
	procRaw.sys_time_1 = prusage.pr_stime.tv_sec;
	procRaw.sys_time_2 = prusage.pr_stime.tv_nsec;

		// reset our privledges
	set_priv( priv );

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
	pi->user_time   = procRaw.user_time_1 / HZ;
	pi->sys_time    = procRaw.sys_time_1  / HZ;
	double cpu_time = (procRaw.user_time_1 + procRaw.sys_time_1) / (double)HZ;

		// convert the creation time from jiffies since boot
		// to epoch time
	
		// 1. ensure we have a "good" boottime
	if( checkBootTime(procRaw.sample_time) == PROCAPI_FAILURE ){
		status = PROCAPI_UNSPECIFIED;
		dprintf( D_ALWAYS, "ProcAPI: Problem getting boottime\n");
		return PROCAPI_FAILURE;
	}
	
		// 2. Convert from jiffies to seconds
	long bday_secs = procRaw.creation_time / HZ;
	
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

		// up our privledges
	priv_state priv = set_root_priv();
	

	// read the entry a certain number of times since it appears that linux
	// often simply does something stupid while reading.
	sprintf( path, "/proc/%d/stat", pid );
	number_of_attempts = 0;
	while (number_of_attempts < num_attempts) {

		number_of_attempts++;

		// in case I must restart, assume that everything is ok again...
		status = PROCAPI_OK;
		initProcInfoRaw(procRaw);

		if( (fp = fopen(path, "r")) == NULL ) {
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
			"%lu %lu %ld %lu %lu %lu %lu %lu %lu %lu %lu "
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

		set_priv( priv );

		return PROCAPI_FAILURE;
	}

	// grab the process owner uid
	procRaw.owner = getFileOwner(fileno(fp));

		// close the file
	fclose( fp );

		// only one value for times
	procRaw.user_time_2 = 0;
	procRaw.sys_time_2 = 0;
	
		// relower our privledges
	set_priv( priv );

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
	char *env_tmp;
	int multiplier = 2;

		// up our privledges
	priv_state priv = set_root_priv();

		// open the environment proc file
	sprintf( path, "/proc/%d/environ", pi->pid );
	fd = open(path, O_RDONLY);

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
				env_tmp = (char*)realloc(env_buffer, read_size * multiplier);
				if ( env_tmp == NULL ) {
					EXCEPT( "Procapi::getProcInfo: Out of memory!\n");
				}
				free(env_buffer);
				env_buffer = env_tmp;
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

		//relower our permissions
	set_priv( priv );

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
		if( (fp = fopen("/proc/uptime","r")) ) {
			double uptime=0;
			double junk=0;
			fgets( s, 256, fp );
			if (sscanf( s, "%lf %lf", &uptime, &junk ) >= 1) {
				// uptime is number of seconds since boottime
				// convert to nearest time stamp
				uptime_boottime = (unsigned long)(now - uptime + 0.5);
			}
			fclose( fp );
		}

		// get stat_boottime
		if( (fp = fopen("/proc/stat", "r")) ) {
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

		// up our privledges
	priv_state priv = set_root_priv();

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

		set_priv( priv );

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

		// relower our permissions
	set_priv( priv );

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
	pi->pid = procRaw.pid;
	pi->ppid = procRaw.ppid;
	pi->owner = procRaw.owner;
	
		// convert the number of page faults into a rate
	do_usage_sampling(pi, cpu_time, procRaw.majfault, procRaw.minfault);

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
		status = PROCAPI_UNSPECIFIED;

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
		status = PROCAPI_UNSPECIFIED;

		dprintf( D_FULLDEBUG, 
			"ProcAPI: sysctl() (pass 2) on pid %d failed with %d(%s)\n",
			pid, errno, strerror(errno) );

		free(kp);

        return PROCAPI_FAILURE;
    }

	// Now, for some things, we're going to have to go to Mach and ask
	// it what it knows - for example, the image size and friends
	task_port_t task;

	kern_return_t results;
	results = task_for_pid(mach_task_self(), pid, &task);
	if(results != KERN_SUCCESS) {
		status = PROCAPI_UNSPECIFIED;

		dprintf( D_FULLDEBUG, 
			"ProcAPI: task_port_pid() on pid %d failed with %d(%s)\n",
			pid, results, mach_error_string(results) );

		free(kp);

        return PROCAPI_FAILURE;
	}

	task_basic_info 	ti;
	unsigned int 		count;	
	
	count = TASK_BASIC_INFO_COUNT;	
	results = task_info(task, TASK_BASIC_INFO, (task_info_t)&ti, &count);  
	if(results != KERN_SUCCESS) {
		status = PROCAPI_UNSPECIFIED;

		dprintf( D_FULLDEBUG, 
			"ProcAPI: task_info() on pid %d failed with %d(%s)\n",
			pid, results, mach_error_string(results) );

		mach_port_deallocate(mach_task_self(), task);
		free(kp);

        return PROCAPI_FAILURE;
	}

		// Fill in procRaw
	procRaw.imgsize = (u_long)ti.virtual_size;
	procRaw.rssize = ti.resident_size;
	procRaw.user_time_1 = ti.user_time.seconds;
	procRaw.user_time_2 = 0;
	procRaw.sys_time_1 = ti.system_time.seconds;
	procRaw.sys_time_2 = 0;
	procRaw.creation_time = kp->kp_proc.p_starttime.tv_sec;
	procRaw.pid = pid;
	procRaw.ppid = kp->kp_eproc.e_ppid;
	procRaw.owner = kp->kp_eproc.e_pcred.p_ruid; 

		// We don't know the page faults
	procRaw.majfault = 0;
	procRaw.minfault = 0;

		// clean up
	mach_port_deallocate(mach_task_self(), task);
	free(kp);

		// success
	return PROCAPI_SUCCESS;
}

#elif defined(CONDOR_FREEBSD)

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

int
ProcAPI::getProcInfo( pid_t pid, piPTR& pi, int &status ) 
{

	// First, let's get the BSD task info for this stucture. This
	// will tell us things like the pid, ppid, etc. 

	int mib[4];
	struct kinfo_proc *kp, *kprocbuf;
	size_t bufSize = 0;

	status = PROCAPI_OK;

	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC;    
	mib[2] = KERN_PROC_PID;
	mib[3] = pid;

	if (sysctl(mib, 4, NULL, &bufSize, NULL, 0) < 0) {
		status = PROCAPI_UNSPECIFIED;

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
		status = PROCAPI_UNSPECIFIED;

		dprintf( D_FULLDEBUG, 
			"ProcAPI: sysctl() (pass 2) on pid %d failed with %d(%s)\n",
			pid, errno, strerror(errno) );

		free(kp);

		return PROCAPI_FAILURE;
	}

	FILE* fp;
	char path[MAXPATHLEN];
	struct procstat prs;
	sprintf(path,"/proc/%d/status",pid);
	if( (fp = fopen( path, "r" )) != NULL ){
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
	// This *could* allocate memory and make pi point to it if pi == NULL.
	// It is up to the caller to get rid of it.
		initpi(pi);
		struct vmspace vm = (kp->kp_eproc).e_vm; 
		pi->imgsize = vm.vm_map.size / 1024;
		pi->rssize = vm.vm_pmap.pm_stats.resident_count * getpagesize();
		pi->user_time = prs.utime;
		pi->sys_time = prs.stime;
		pi->creation_time = prs.start;
		pi->age = secsSinceEpoch() - pi->creation_time; 
		pi->pid = pid;
		pi->ppid = kp->kp_eproc.e_ppid;
		pi->owner = kp->kp_eproc.e_pcred.p_ruid; 

		long nowminf, nowmajf;
		double ustime = pi->user_time + pi->sys_time;

		nowminf = 0;
		nowmajf = 0;
		do_usage_sampling(pi, ustime, nowmajf, nowminf);
		fclose(fp);
		free(kp);
	  return PROCAPI_SUCCESS;
	}
	dprintf( D_FULLDEBUG, "ProcAPI: /proc/%d/status not found!\n", pid);
	free(kp);
	return PROCAPI_FAILURE;

}

int
ProcAPI::getProcInfoRaw( pid_t pid, procInfoRaw& procRaw, int &status ) 
{

	int mib[4];
	struct kinfo_proc *kp, *kprocbuf;
	size_t bufSize = 0;

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
		status = PROCAPI_UNSPECIFIED;

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
		status = PROCAPI_UNSPECIFIED;

		dprintf( D_FULLDEBUG, 
			"ProcAPI: sysctl() (pass 2) on pid %d failed with %d(%s)\n",
			pid, errno, strerror(errno) );

		free(kp);

        return PROCAPI_FAILURE;
    }
	FILE* fp;
	char path[MAXPATHLEN];
	struct procstat prs;
	sprintf(path,"/proc/%d/status",pid);
	if( (fp = fopen( path, "r" )) != NULL ){
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
	// This *could* allocate memory and make pi point to it if pi == NULL.
	// It is up to the caller to get rid of it.
		struct vmspace vm = (kp->kp_eproc).e_vm; 
		procRaw.imgsize = vm.vm_map.size / 1024;
		procRaw.rssize = vm.vm_pmap.pm_stats.resident_count * getpagesize();
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

		free(kp);

		// success
		return PROCAPI_SUCCESS;
	}
	dprintf( D_FULLDEBUG, "ProcAPI: /proc/%d/status not found!\n", pid);
	free(kp);
	return PROCAPI_FAILURE;
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
	pi->imgsize   = procRaw.imgsize / 1024;
    pi->rssize    = procRaw.rssize / 1024;

		// convert the time fields from the object time scale
		// to seconds
	pi->user_time 		= (long) (procRaw.user_time_1 / procRaw.object_frequency);
    pi->sys_time  		= (long) (procRaw.sys_time_1 / procRaw.object_frequency);
	pi->creation_time 	= (long) (procRaw.creation_time / procRaw.object_frequency);
	double cpu_time = procRaw.cpu_time / procRaw.object_frequency;
	double now_secs = procRaw.sample_time / procRaw.object_frequency;

		// calculate the age
	double age_wrong_scale = procRaw.sample_time - ((double)procRaw.creation_time);
	pi->age = (long)(age_wrong_scale / procRaw.object_frequency);
	
			// copy the remainder of the field
	pi->pid       = procRaw.pid;
    pi->ppid      = procRaw.ppid;
	pi->minfault  = 0;   // not supported by NT; all faults lumped into major.
	
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

    DWORD dwStatus;  // return status of fn. calls

/* Note to whom it may concern:  we DO NOT need to set_root_priv here!
   NT makes this stuff available to any old process.  I've left this
   in and commented out for demonstration purposes, and have removed
   the rest of the priv stuff from the NT code. */
//	priv_state priv = set_root_priv();

        // '2' is the 'system' , '230' is 'process'  
        // I hope these numbers don't change over time... 
    dwStatus = GetSystemPerfData ( TEXT("230") );

    if( dwStatus != ERROR_SUCCESS ) {
        dprintf( D_ALWAYS, "ProcAPI: getProcInfo() failed to get "
				 "performance info.\n");
		status = PROCAPI_UNSPECIFIED;
        return PROCAPI_FAILURE;
    }

        // somehow we don't have the process data -> panic
    if( pDataBlock == NULL ) {
        dprintf( D_ALWAYS, "ProcAPI: getProcInfo() failed to make "
				 "pDataBlock.\n");
		status = PROCAPI_UNSPECIFIED;
        return PROCAPI_FAILURE;
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

    procRaw.pid       = (long) *((long*)(ctrblk + offsets->procid  ));
    procRaw.ppid      = ntSysInfo.GetParentPID(pid);
    procRaw.imgsize   = (long) (*((long*)(ctrblk + offsets->imgsize )));
    procRaw.rssize    = (long) (*((long*)(ctrblk + offsets->rssize  )));

	procRaw.majfault  = (long) *((long*)(ctrblk + offsets->faults  ));
	procRaw.minfault  = 0;  // not supported by NT; all faults lumped into major.
	procRaw.user_time_1 = (long) (LI_to_double( ut ));
	procRaw.user_time_2 = 0;
    procRaw.sys_time_1  = (long) (LI_to_double( st ));
	procRaw.sys_time_2 = 0;

	procRaw.creation_time = (long) (LI_to_double( elt ));
	procRaw.cpu_time	=  LI_to_double( pt );

		// set the sample time and object frequency
	procRaw.sample_time = sampleObjectTime;
	procRaw.object_frequency = objectFrequency;

		// success
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
	pi->imgsize = procRaw.imgsize * getpagesize();


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

#ifndef WIN32
/* getProcSetInfo returns the sum of the procInfo structs for a set
   of pids.  These pids are specified by an array of pids ('pids') that
   has 'numpids' elements. */
int
ProcAPI::getProcSetInfo( pid_t *pids, int numpids, piPTR& pi, int &status ) 
{
	piPTR temp = NULL;
	int val = 0;
	priv_state priv;
	int info_status;
	int fatal_failure = FALSE;

	// This *could* allocate memory and make pi point to it if 
	// pi == NULL. It is up to the caller to get rid of it.
	initpi ( pi );

	status = PROCAPI_OK;

	if( numpids <= 0 || pids == NULL ) {
			// We were passed nothing, so there's no work to do and we
			// should return immediately before we dereference
			// something we shouldn't be touching.
		return PROCAPI_SUCCESS;
	}

	priv = set_root_priv();

	for( int i=0; i<numpids; i++ ) {

		val = getProcInfo( pids[i], temp, info_status );

		switch( val ) {
		
			case PROCAPI_SUCCESS:

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

			case PROCAPI_FAILURE:

				switch(info_status) {

					case PROCAPI_NOPID:
						/* We warn about this error, but ignore it since if 
							this pid isn't around, we don't compute any
							usage information for it. */

						dprintf( D_FULLDEBUG, 	
								"ProcAPI::getProcSetInfo(): Pid %d does "
					 			"not exist, ignoring.\n", pids[i] );

						break;

					case PROCAPI_PERM:
						/* The known methods why can happen are that 
							getPidFamily() returned something bogus, or
							a race condition is hit that signifies a
							situation between when a pid was alive in the 
							pidfamily array when this function was entered,
							but exited and was replaced by another pid owned
							by someone else before this for loop got to it
							in the array. So, we'll warn on it, but it won't
							be a fatal error. */

						dprintf( D_FULLDEBUG, 
								"ProcAPI::getProcSetInfo(): Suspicious "
								"permission error getting info for pid %lu.\n",
								(unsigned long)pids[i] );
						break;

					default:
						dprintf( D_ALWAYS, 
							"ProcAPI::getProcSetInfo(): Unspecified return "
					 		"status (%d) from a failed getProcInfo(%lu)\n", 
							info_status, (unsigned long)pids[i] );

						fatal_failure = TRUE;

					break;
				}
			break;

			default:
				EXCEPT("ProcAPI::getProcSetInfo(): Invalid return code. "
						"Programmer error!");
				break;
		}
	}

	if( temp ) { 
		delete temp;
	}

	set_priv( priv );

	if (fatal_failure == TRUE) {
		// Don't really know how to group the various failures of the above code
		status = PROCAPI_UNSPECIFIED;
		return PROCAPI_FAILURE;
	}

	return PROCAPI_SUCCESS;
}
#endif

#ifdef WIN32
// There is a much better way to do this in WIN32...we get all instances
// of all processes at once anyway...
int
ProcAPI::getProcSetInfo( pid_t *pids, int numpids, piPTR& pi, int &status ) {

	// This *could* allocate memory and make pi point to it if 
	// pi == NULL. It is up to the caller to get rid of it.
    initpi( pi );

	status = PROCAPI_OK;

	if( numpids <= 0 || pids == NULL ) {
			// We were passed nothing, so there's no work to do and we
			// should return immediately before we dereference
			// something we shouldn't be touching.
		return PROCAPI_SUCCESS;
	}

	DWORD dwStatus;  // return status of fn. calls

        // '2' is the 'system' , '230' is 'process'  
        // I hope these numbers don't change over time... 
    dwStatus = GetSystemPerfData( TEXT("230") );
    
    if( dwStatus != ERROR_SUCCESS ) {
        dprintf( D_ALWAYS, "ProcAPI::getProcSetInfo failed to get "
				 "performance info.\n");

		status = PROCAPI_UNSPECIFIED;
        return PROCAPI_FAILURE;
    }

        // somehow we don't have the process data -> panic
    if( pDataBlock == NULL ) {
        dprintf( D_ALWAYS, "ProcAPI::getProcSetInfo failed to make "
				 "pDataBlock.\n");

		status = PROCAPI_UNSPECIFIED;
        return PROCAPI_FAILURE;
    }
    
    PPERF_OBJECT_TYPE pThisObject = firstObject (pDataBlock);

    if( !offsets ) {	// If we haven't yet gotten the offsets, grab 'em.
        grabOffsets( pThisObject );
    }

        // create local copy of pids.
	pid_t *localpids = (pid_t*) malloc ( sizeof (pid_t) * numpids );
	if (localpids == NULL) {
		EXCEPT( "ProcAPI:getProcSetInfo: Out of Memory!");
	}

	for( int i=0 ; i<numpids ; i++ ) {
		localpids[i] = pids[i];
	}
	
	multiInfo( localpids, numpids, pi );

	free(localpids);

	return PROCAPI_SUCCESS;
}


int
ProcAPI::getFamilyInfo ( pid_t daddypid, piPTR& pi, PidEnvID *penvid,
			int &status ) 
{

	status = PROCAPI_OK;

	// This *could* allocate memory and make pi point to it if 
	// pi == NULL. It is up to the caller to get rid of it.
    initpi ( pi );

	if ( daddypid == 0 ) {
		return PROCAPI_SUCCESS;
	}
	
	DWORD dwStatus;  // return status of fn. calls

		// '2' is the 'system' , '230' is 'process'  
        // I hope these numbers don't change over time... 
    dwStatus = GetSystemPerfData ( TEXT("230") );
    
    if ( dwStatus != ERROR_SUCCESS ) {
        dprintf( D_ALWAYS, "ProcAPI::getProcSetInfo failed to get "
				 "performance info.\n");
		status = PROCAPI_UNSPECIFIED;
        return PROCAPI_FAILURE;
    }

        // somehow we don't have the process data -> panic
    if ( pDataBlock == NULL ) {
        dprintf( D_ALWAYS, "ProcAPI::getProcSetInfo failed to make "
				 "pDataBlock.\n");
		status = PROCAPI_UNSPECIFIED;
        return PROCAPI_FAILURE;
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
	makeFamily( daddypid, penvid, allpids, numpids, familypids, familysize );

	multiInfo( familypids, familysize, pi );

	delete [] allpids;
	delete [] familypids;

	return PROCAPI_SUCCESS;
}

int
ProcAPI::getPidFamily( pid_t daddypid, PidEnvID *penvid, pid_t *pidFamily, 
	int &status ) 
{

	status = PROCAPI_FAMILY_ALL;

	if ( daddypid == 0 ) {
		pidFamily[0] = 0;
		return PROCAPI_SUCCESS;
	}

	DWORD dwStatus;  // return status of fn. calls

	if ( pidFamily == NULL ) {
		dprintf( D_ALWAYS, 
				 "ProcAPI::getPidFamily: no space allocated for pidFamily\n" );
		status = PROCAPI_UNSPECIFIED;
        return PROCAPI_FAILURE;
	}

        // '2' is the 'system' , '230' is 'process'  
        // I hope these numbers don't change over time... 
    dwStatus = GetSystemPerfData ( TEXT("230") );
    
	if ( dwStatus != ERROR_SUCCESS ) {
        dprintf( D_ALWAYS, 
				 "ProcAPI::getProcSetInfo failed to get performance info.\n");
		status = PROCAPI_UNSPECIFIED;
        return PROCAPI_FAILURE;
    }

        // somehow we don't have the process data -> panic
    if ( pDataBlock == NULL ) {
        dprintf( D_ALWAYS, 
				 "ProcAPI::getProcSetInfo failed to make pDataBlock.\n");
		status = PROCAPI_UNSPECIFIED;
        return PROCAPI_FAILURE;
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
	makeFamily( daddypid, penvid, allpids, numpids, familypids, familysize );
	
	for ( int q=0 ; q<familysize ; q++ ) {
		pidFamily[q] = familypids[q];
	}

	pidFamily[familysize] = 0;

	delete [] allpids;
	delete [] familypids;

	return PROCAPI_SUCCESS;
}

void
ProcAPI::makeFamily( pid_t dadpid, PidEnvID *penvid, pid_t *allpids, 
		int numpids, pid_t* &fampids, int &famsize ) 
{

	pid_t *parentpids = new pid_t[numpids];
	fampids = new pid_t[numpids];  // just might include everyone...

	for( int i=0 ; i<numpids ; i++ ) {
		parentpids[i] = ntSysInfo.GetParentPID( allpids[i] );
	}

	famsize = 1;
	fampids[0] = dadpid;
	int numadditions = 1;

	CSysinfo csi;

	procInfo pi;

	while( numadditions > 0 ) {
		numadditions = 0;
		for( int j=0; j<numpids; j++ ) {
			
			// only need to initialize these aspects of the procInfo for 
			// isinfamily().
			pi.ppid = parentpids[j];
			pi.pid = allpids[j];
			pidenvid_init(&pi.penvid);

			if( isinfamily(fampids, famsize, penvid, &pi) ) {

				// now make sure the parent pid is older than its child
				if (csi.ComparePidAge(allpids[j], parentpids[j]) == -1 ) {

					dprintf(D_FULLDEBUG, "ProcAPI: pid %d is older than "
							"its (presumed) parent (%d), so we're leaving it "
						   "out.\n", allpids[j], parentpids[j]);	

				} else { 
				
					fampids[famsize] = allpids[j];
					parentpids[j] = 0;
					allpids[j] = 0;
					famsize++;
					numadditions++;
				}
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
ProcAPI::getPidFamily( pid_t pid, PidEnvID *penvid, pid_t *pidFamily, 
	int &status )
{
	int fam_status;
	int rval;

  // I'm going to do this in a somewhat ugly hacked way....get all the info
  // instead of just pids...but it's a lot quicker to write.

	if( pidFamily == NULL ) {
		dprintf( D_ALWAYS, 
				 "ProcAPI::getPidFamily: no space allocated for pidFamily\n" );

		status = PROCAPI_UNSPECIFIED;
		return PROCAPI_FAILURE;
	}

#ifndef HPUX
	buildPidList();
#endif

	buildProcInfoList();

	rval = buildFamily(pid, penvid, fam_status);

	switch (rval)
	{
		case PROCAPI_SUCCESS:
			switch (fam_status)
			{
				case PROCAPI_FAMILY_ALL:
					status = PROCAPI_FAMILY_ALL;
					break;

				case PROCAPI_FAMILY_SOME:
					status = PROCAPI_FAMILY_SOME;
					break;

				default:
					/* This shouldn't happen, but if it does */
					EXCEPT( "ProcAPI::buildFamily() returned an "
						"incorrect status on success! Programmer error!\n" );
					break;
			}
			
			break;

		case PROCAPI_FAILURE:

			// no family at all found, clean up and get out 

			deallocPidList();
			deallocAllProcInfos();
			deallocProcFamily();

			status = PROCAPI_FAMILY_NONE;
			return PROCAPI_FAILURE;

			break;

		default:
			break;
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

	return PROCAPI_SUCCESS;
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
ProcAPI::getFamilyInfo( pid_t daddypid, piPTR& pi, PidEnvID *penvid, 
		int &status ) 
{
	int fam_status;
	int rval;

	// assume I'll find everything I was asked for.
	status = PROCAPI_FAMILY_ALL;

#ifndef HPUX        // everyone except HPUX needs a pidlist built.
	buildPidList();
#endif

	buildProcInfoList();  // HPUX has its own version of this, too.

	rval = buildFamily(daddypid, penvid, fam_status);
	switch (rval)
	{
		case PROCAPI_SUCCESS:
			switch (fam_status)
			{
				case PROCAPI_FAMILY_ALL:
					status = PROCAPI_FAMILY_ALL;
					break;

				case PROCAPI_FAMILY_SOME:
					status = PROCAPI_FAMILY_SOME;
					break;

				default:
					/* This shouldn't happen, but if it does */
					EXCEPT( "ProcAPI::getFamilyInfo() returned an "
						"incorrect status on success! Programmer error!\n" );
					break;
			}
			
			break;

		case PROCAPI_FAILURE:

			// no family at all found, clean up and get out 

			deallocPidList();
			deallocAllProcInfos();
			deallocProcFamily();

			status = PROCAPI_FAMILY_NONE;
			return PROCAPI_FAILURE;

			break;

		default:
			/* nothing to do */
			break;
	}

		// now, procFamily points to a list of the procInfos in that
		// family. 
	// This *could* allocate memory and make pi point to it if 
	// pi == NULL. It is up to the caller to get rid of it.
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

	return PROCAPI_SUCCESS;
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
	pi->owner    = 0;

	pidenvid_init(&pi->penvid);
}

void
ProcAPI::initProcInfoRaw(procInfoRaw& procRaw){
	memset(&procRaw, 0, sizeof(procInfoRaw));
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

		return PROCAPI_SUCCESS;
	} 

	delete pidList;        // remove header node.
	pidList = NULL;

	set_priv( priv );

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

	priv_state priv = set_root_priv();

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

	if (sysctl(mib, 4, NULL, &bufSize, NULL, 0) < 0) {
		 //perror("Failure calling sysctl");
		set_priv( priv );
		return PROCAPI_FAILURE;
	}	

	kprocbuf = kp = (struct kinfo_proc *)malloc(bufSize);

	origBufSize = bufSize;
	if ( sysctl(mib, 4, kp, &bufSize, NULL, 0) < 0) {
		free(kprocbuf);
		set_priv( priv );
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

	set_priv( priv );

	return PROCAPI_SUCCESS;
}

#endif

#ifdef CONDOR_FREEBSD
int
ProcAPI::buildPidList() {

	pidlistPTR current;
	pidlistPTR temp;

	priv_state priv = set_root_priv();

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
		set_priv( priv );
		return PROCAPI_FAILURE;
	}	

	kprocbuf = kp = (struct kinfo_proc *)malloc(bufSize);

	origBufSize = bufSize;
	if ( sysctl(mib, 3, kp, &bufSize, NULL, 0) < 0) {
		free(kprocbuf);
		set_priv( priv );
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

	set_priv( priv );

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

			pi->imgsize = pent[i].pi_size * getpagesize();
			pi->rssize = pent[i].pi_drss + pent[i].pi_trss; /* XXX not really right */
			pi->minfault = pent[i].pi_minflt;
			pi->majfault = pent[i].pi_majflt;
			pi->user_time = pent[i].pi_ru.ru_utime.tv_usec; /* XXX fixme microseconds */
			pi->sys_time = pent[i].pi_ru.ru_stime.tv_usec; /* XXX fixme microseconds */
			pi->age = secsSinceEpoch() - pent[i].pi_start;
			pi->pid = pent[i].pi_pid;
			pi->ppid = pent[i].pi_ppid;
			pi->creation_time = pent[i].pi_start;

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
	}

		// we're done; remove header node.
	temp = allProcInfos;
	allProcInfos = allProcInfos->next;
	delete temp;

	return PROCAPI_SUCCESS;
}
#endif

/* buildFamily takes a list of procInfo structs pointed to by 
   allProcInfos and an ancestor pid 'daddypid' and builds a list
   of all procInfo structs and points 'procFamily' to it.
*/
int
ProcAPI::buildFamily( pid_t daddypid, PidEnvID *penvid, int &status ) {

		// array of pids in family.  Size # pids.
	pid_t *familypids;
	int familysize = 0;
	int numprocs;

	// assume that I'm going to find the daddy and all of the descendants...
	status = PROCAPI_FAMILY_ALL;

	if( (DebugFlags & D_FULLDEBUG) && (DebugFlags & D_PROCFAMILY) ) {
		dprintf( D_FULLDEBUG, 
				 "ProcAPI::buildFamily() called w/ parent: %d\n", daddypid );
	}

	numprocs = getNumProcs();
	deallocProcFamily();
	procFamily = NULL;

		// make an array of size # processes for quick lookup of pids in family
	familypids = new pid_t[numprocs];

		// get the daddypid's procInfo struct
	piPTR pred, current, familyend;

	// find the daddy pid out of the linked list of all of the processes
	current = allProcInfos;
	while( (current != NULL) && (current->pid != daddypid) ) {
		pred = current;
		current = current->next;
	}

	if (current != NULL) {
		dprintf( D_FULLDEBUG, 
			"ProcAPI::buildFamily() Found daddypid on the system: %u\n", 
			current->pid );
	}

	// if the daddy pid isn't in the list, then check and see if there are any
	// children of that pid. If so, pick one to be the conceptual daddy of the
	// rest of them regardless of actual parentage.
	if( current == NULL ) {
		current = allProcInfos;
		while( (current != NULL) && 
				(pidenvid_match(penvid, &current->penvid) != PIDENVID_MATCH))
		{
			pred = current;
			current = current->next;
		}

		if (current != NULL) {

			// found something that was most likely a descendant of daddypid
			status = PROCAPI_FAMILY_SOME;

			dprintf(D_FULLDEBUG, 
				"ProcAPI::buildFamily() Parent pid %u is gone. "
				"Found descendant %u via ancestor environment "
				"tracking and assigning as new \"parent\".\n", 
				daddypid, current->pid);
		}
	}

	// if we couldn't find the daddy pid we were originally looking for, or 
	// any children of said pid (which are members of the daddy pid's family
	// according to the ancestor environment variables condor supplies)
	// then truly give up.
	if( current == NULL ) {

		delete [] familypids;

		dprintf( D_FULLDEBUG, "ProcAPI::buildFamily failed: parent %d "
				 "not found on system.\n", daddypid );

		status = PROCAPI_FAMILY_NONE;

		return PROCAPI_FAILURE;
	}

	// In the case where we truly found the daddy pid, then procFamily will
	// point to it. In the case where we found a child of the daddy pid, then
	// that (arbitrarily chosen) child will become the daddy pid in spirit.

		// special case: daddy is first on list:
	if( allProcInfos == current ) {
		procFamily = allProcInfos;
		allProcInfos = allProcInfos->next;
		procFamily->next = NULL;
	} else {  // regular case: daddy somewhere in middle
		procFamily = current;
		pred->next = current->next;
		procFamily->next = NULL;
	}

	// take whatever we found as a daddy pid proc structure and use that pid.
	// the caller expects the oth index of this array to contain the daddy pid.
	familypids[0] = current->pid;
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
			if( isinfamily(familypids, familysize, penvid, current) ) {
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

	return PROCAPI_SUCCESS;
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
ProcAPI::isinfamily( pid_t *fam, int size, PidEnvID *penvid, piPTR child ) 
{
	for( int i=0; i<size; i++ ) {
		// if the child's parent pid is one of the values in the family, then 
		// the child is actually a child of one of the pids in the fam array.
		if( child->ppid == fam[i] ) {

			if( (DebugFlags & D_FULLDEBUG) && (DebugFlags & D_PROCFAMILY) ) {
				dprintf( D_FULLDEBUG, "Pid %u is in family of %u\n", 
					child->pid, fam[i] );
			}

			return true;
		}

		// check to see if the daddy's ancestor pids are a subset of the
		// child's, if so, then the child is a descendent of the daddy pid.
		if (pidenvid_match(penvid, &child->penvid) == PIDENVID_MATCH) {

			if( (DebugFlags & D_FULLDEBUG) && (DebugFlags & D_PROCFAMILY) ) {
				dprintf( D_FULLDEBUG, 
					"Pid %u is predicted to be in family of %u\n", 
					child->pid, fam[i] );
			}

			return true;
		}
	}

	return false;
}

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
	fprintf( fp, "Times:  user, system, age: %ld %ld %ld\n", 
			pi->user_time, pi->sys_time, pi->age );
	fprintf( fp, "percent cpu usage of this process: %5.2f\n", pi->cpuusage);
	fprintf( fp, "pid is %d, ppid is %d\n", pi->pid, pi->ppid);
	fprintf( fp, "\n" );
}

#ifndef WIN32

uid_t 
ProcAPI::getFileOwner(int fd) {
	
	struct stat si;

	if ( fstat(fd, &si) != 0 ) {
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

int
ProcAPI::getPidFamilyByLogin( const char *searchLogin, pid_t *pidFamily )
{
#ifndef WIN32

	// first, get the Login's uid, since that's what's stored in
	// the ProcInfo structure.
	ASSERT(pidFamily);
	ASSERT(searchLogin);
	struct passwd *pwd = getpwnam(searchLogin);
	uid_t searchUid = pwd->pw_uid;

	// now iterate through allProcInfos to find processes
	// owned by the given uid
	piPTR cur = allProcInfos;
	int fam_index = 0;

#ifndef HPUX        // everyone except HPUX needs a pidlist built.
	buildPidList();
#endif

	buildProcInfoList();  // HPUX has its own version of this, too.

	while ( cur != NULL ) {
		if ( cur->owner == searchUid ) {
			dprintf(D_PROCFAMILY,
				  "ProcAPI: found pid %d owned by %s (uid=%d)\n",
				  cur->pid, searchLogin, searchUid);
			pidFamily[fam_index] = cur->pid;
			fam_index++;
		}
		cur = cur->next;
	}
	pidFamily[fam_index] = (pid_t)0;
	
	return PROCAPI_SUCCESS;
#else
	// Win32 version
	ExtArray<pid_t> pids(256);
	int num_pids;
	int index_pidFamily = 0;
	int index_familyHandles = 0;
	BOOL ret;
	HANDLE procToken;
	HANDLE procHandle;
	TOKEN_INFORMATION_CLASS tic;
	VOID *tokenData;
	DWORD sizeRqd;
	CHAR str[80];
	DWORD strSize;
	CHAR str2[80];
	DWORD str2Size;
	SID_NAME_USE sidType;

	closeFamilyHandles();

	ASSERT(pidFamily);
	ASSERT(searchLogin);

	// get a list of all pids on the system
	num_pids = ntSysInfo.GetPIDs(pids);

	// loop through pids comparing process owner
	for (int s=0; s<num_pids; s++) {

		// find owner for pid pids[s]
		
	// again, this is assumed to be wrong, so I'm 
	// nixing it for now.
	// skip the daddy_pid, we already added it to our list
//		  if ( pids[s] == daddy_pid ) {
//			  continue;
//		  }

		  // get a handle for the process
		  procHandle=OpenProcess(PROCESS_QUERY_INFORMATION,
			FALSE, pids[s]);
		  if (procHandle == NULL)
		  {
			  // Unable to open the process for query - try next pid
			  continue;			
		  }

		  // get a handle for the access token used
		  // by the process
		  ret=OpenProcessToken(procHandle,
			TOKEN_QUERY, &procToken);
		  if (!ret)
		  {
			  // Unable to open the access token.
			  CloseHandle(procHandle);
			  continue;
		  }

		  // ----- Get user information -----

		  // specify to return user info
		  tic=TokenUser;

		  // find out how much mem is needed
		  ret=GetTokenInformation(procToken, tic, NULL, 0,
			&sizeRqd);

		  // allocate that memory
		  tokenData=(TOKEN_USER *) GlobalAlloc(GPTR,
			sizeRqd);
		  if (tokenData == NULL)
		  {
			EXCEPT("Unable to allocate memory.");
		  }

		  // actually get the user info
		  ret=GetTokenInformation(procToken, tic,
			tokenData, sizeRqd, &sizeRqd);
		  if (!ret)
		  {
			// Unable to get user info.
			CloseHandle(procToken);
			GlobalFree(tokenData);
			CloseHandle(procHandle);
			continue;
		  }

		  // specify size of string buffers
		  strSize=str2Size=80;

		  // convert user SID into a name and domain
		  ret=LookupAccountSid(NULL,
			((TOKEN_USER *)tokenData)->User.Sid,
			str, &strSize, str2, &str2Size, &sidType);
		  if (!ret)
		  {
		    // Unable to look up SID.
			CloseHandle(procToken);
			GlobalFree(tokenData);
			CloseHandle(procHandle);
			continue;
		  }

		  // release memory, handles
		  GlobalFree(tokenData);
		  CloseHandle(procToken);

		  // user is now in variable str, domain in str2

		  // see if it is the login prefix we are looking for
		  if ( strincmp(searchLogin,str,strlen(searchLogin))==0 ) {
			  // a match!  add to our list.   
			  pidFamily[index_pidFamily++] = pids[s];
			  // and add the procHandle to a list as well; we keep the
			  // handle open so the pid is not reused by NT between now and
			  // when the caller actually does something with this pid.
			  familyHandles[index_familyHandles++] = procHandle;
			  dprintf(D_PROCFAMILY,
				  "getPidFamilyByLogin: found pid %d owned by %s\n",
				  pids[s],str);
		  } else {
			  // not a match; close the handle to this process
			  CloseHandle(procHandle);
		  }

	}	// end of for loop looping through all pids

	// denote end of list with a zero entry
	pidFamily[index_pidFamily] = 0;

	if ( index_pidFamily > 0 ) {
		// return success
		return PROCAPI_SUCCESS;
	}

	// return failure
	return PROCAPI_FAILURE;

#endif  // of WIN32
}

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
	FILE* fp = fopen("/proc/uptime", "r");
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



void
ProcAPI::closeFamilyHandles()
{
	// This function is a no-op on Unix...
#ifdef WIN32
	int i;
	for ( i=0; i < (familyHandles.getlast() + 1); i++ ) {
		CloseHandle( familyHandles[i] );
	}
	familyHandles.truncate(-1);
#endif // of Win32
	return;
}
