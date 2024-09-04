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

#if HAVE_PROCFS_H
# include <procfs.h>
#endif
#if HAVE_SYS_PROCFS_H
# include <sys/procfs.h>
#endif

#include <sstream>

size_t pidHashFunc( const pid_t& pid );

std::map<pid_t, procHashNode> ProcAPI::procHash;

piPTR ProcAPI::allProcInfos = NULL;

// counters for measuring the performance of GetProcInfoList
//
int    ProcAPI::cGetProcInfoList = 0;
double ProcAPI::sGetProcInfoList = 0.0;
int    ProcAPI::cGetProcInfoListReg = 0;
double ProcAPI::sGetProcInfoListReg = 0.0;
int    ProcAPI::cGetProcInfoListPid = 0;
double ProcAPI::sGetProcInfoListPid = 0.0;
int    ProcAPI::cGetProcInfoListCPU = 0;
double ProcAPI::sGetProcInfoListCPU = 0.0;
int ProcAPI::getProcInfoListStats(double & sOverall, 
                                  int & cReg, double & sReg, 
                                  int & cPid, double & sPid,
                                  int & cCPU, double & sCPU)
{
   cReg = cGetProcInfoListReg;
   sReg = sGetProcInfoListReg;
   cPid = cGetProcInfoListPid;
   sPid = sGetProcInfoListPid;
   cCPU = cGetProcInfoListCPU;
   sCPU = sGetProcInfoListCPU;
   sOverall = sGetProcInfoList;
   return cGetProcInfoList; 
}


#ifndef WIN32
std::vector<pid_t> ProcAPI::pidList;
int ProcAPI::pagesize		= 0;
#ifdef LINUX
long unsigned ProcAPI::boottime	= 0;
long ProcAPI::boottime_expiration = 0;
#endif // LINUX
#else // WIN32

#include <psapi.h> // for GetProcessMemoryInfo
#include "ntsysinfo.WINDOWS.h"
static CSysinfo ntSysInfo;	// for getting parent pid on NT

	// Windows gives us birthday in 100ns ticks since 01/01/1601
	// (11644473600 seconds before 01/01/1970)
const __int64 EPOCH_SHIFT = 11644473600;

PPERF_DATA_BLOCK ProcAPI::pDataBlock	= NULL;
size_t           ProcAPI::cbDataBlockAlloc = 0;
size_t           ProcAPI::cbDataBlockData = 0;
int              ProcAPI::cAllocs = 0;
int              ProcAPI::cReallocs = 0;
int              ProcAPI::cGetSystemPerfDataCalls = 0;
int              ProcAPI::cPerfDataQueries = 0;
double           ProcAPI::sPerfDataQueries = 0.0;

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

ProcAPI::~ProcAPI() {
        // deallocate stuff like crazy.
#ifndef WIN32
    pidList.clear();
#endif
    deallocAllProcInfos();

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

#if defined(LINUX)

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

	pi->imgsize = procRaw.imgsize;  //already in k
	pi->rssize = procRaw.rssize * pagesize;  // pages to k
#if HAVE_PSS
	pi->pssize = procRaw.pssize; // k
	pi->pssize_available = procRaw.pssize_available;
#endif

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

#if HAVE_PSS
int
ProcAPI::getPSSInfo( pid_t pid, procInfo& procRaw, int &status ) 
{
	char path[64];
	FILE *fp;
	int number_of_attempts;
	const int max_attempts = 5;

	char *use_pss;
	use_pss = getenv("_condor_USE_PSS");
	if ((use_pss == 0) || (*use_pss == 'f') || (*use_pss == 'F')) {
		return PROCAPI_SUCCESS;
	}

		// Note that HAVE_PSS may be true at compile-time, but that
		// does not mean /proc/pid/smaps will actually contain
		// Pss info at run-time.  Therefore, we do not treat missing
		// Pss info as an error in this function.

	sprintf( path, "/proc/%d/smaps", pid );
	number_of_attempts = 0;
	while (number_of_attempts < max_attempts) {

		number_of_attempts++;

		// in case I must restart, assume that everything is ok again...
		status = PROCAPI_OK;
		procRaw.pssize = 0;
		procRaw.pssize_available = false;

		if( (fp = safe_fopen_wrapper_follow(path, "r")) == NULL ) {
			if( errno == ENOENT ) {
				// /proc/pid doesn't exist
				// This system may simply not support smaps, so
				// don't treat this as an error.  We just won't
				// set pssize_available = true.
				status = PROCAPI_OK;
				dprintf( D_FULLDEBUG, 
					"ProcAPI::getProcInfo() %s does not exist.\n", path );
				break;
			} else if ( errno == EACCES ) {
				status = PROCAPI_PERM;
				dprintf( D_FULLDEBUG, 
					"ProcAPI::getProcInfo() No permission to open %s.\n", 
					 path );
				break;
			} else { 
				status = PROCAPI_UNSPECIFIED;
				dprintf( D_ALWAYS, 
					"ProcAPI::getProcInfo() Error opening %s, errno: %d.\n", 
					 path, errno );
			}

			// immediate failure, try again.
			continue;
		}

		char buf[512];
		while( fgets(buf,sizeof(buf)-1,fp) ) {
			buf[sizeof(buf)-2] = '\0'; // ensure null termination

			if( strncmp(buf,"Pss:",4)==0 ) {
				char const *s = buf+4;
				while( isspace(*s) ) {
					s++;
				}
				char *endptr = NULL;
				unsigned long pssize = (unsigned long)strtol(s,&endptr,10);
				if( !endptr || endptr == s ) {
					dprintf(D_FULLDEBUG,"Unexpted Pss value in %s: %s",path,buf);
					break;
				}
				while( isspace(*endptr) ) {
					endptr++;
				}
				if( strncmp(endptr,"kB",2)!=0 ) {
					dprintf(D_FULLDEBUG,"Unexpted Pss units in %s: %s",path,buf);
					break;
				}

				procRaw.pssize += pssize;
				procRaw.pssize_available = true;
			}
		}

		if( !ferror(fp) ) {
			break;
		}

			// we encountered a read error
		status = PROCAPI_UNSPECIFIED;
		dprintf( D_ALWAYS, 
				 "ProcAPI: Unexpected error on %s, errno: %d.\n", 
				 path, errno );

			// don't leak for the next attempt;
		fclose( fp );
		fp = NULL;

	} 	// end of while number_of_attempts < 0

	if (fp != NULL) {
		fclose( fp );
		fp = NULL;
	}

	if (status == PROCAPI_OK) {
		return PROCAPI_SUCCESS;
	} else {
		return PROCAPI_FAILURE;
	}
}
#endif

size_t ProcAPI::getBasicUsage(pid_t pid, double * puser_time, double * psys_time)
{
	int status;
	procInfoRaw procRaw;
	int retVal = ProcAPI::getProcInfoRaw(pid, procRaw, status);
	if (retVal != 0){
		//on failure, set everything to 0.
		initProcInfoRaw(procRaw);
	}

	long hertz = 1;
# if defined(HZ)
	hertz = HZ;
# elif defined(_SC_CLK_TCK)
	hertz = sysconf(_SC_CLK_TCK);
# else
#   error "Unable to determine system clock"
# endif

	if (puser_time) {
		*puser_time = procRaw.user_time_1 / (double)hertz;
	}
	if (psys_time) {
		*psys_time = procRaw.sys_time_1  / (double)hertz;
	}

	return (size_t)procRaw.imgsize * 1024;
}

/* Fills in procInfoRaw with the following units:
   imgsize		: kbytes
   rssize		: pages
   pssize       : k
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
	unsigned long long imgsize_bytes;
	char c;
	char s[256];
	int num_attempts = 5;

		// assume success
	status = PROCAPI_OK;

	// read the entry a certain number of times since it appears that linux
	// often simply does something stupid while reading.
	sprintf( path, "/proc/%d/stat", pid );
	number_of_attempts = 0;
	while (number_of_attempts < num_attempts) {

		number_of_attempts++;

		// in case I must restart, assume that everything is ok again...
		status = PROCAPI_OK;
		// clear the memory of procRaw
		initProcInfoRaw(procRaw);
		// set the sample time
		procRaw.sample_time = secsSinceEpoch();

		if( (fp = safe_fopen_wrapper_follow(path, "r")) == NULL ) {
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
			
			// if status is NOPID or PERM, just break out of the
			// retry loop, as these errors are likely to persist. gt #3323
			if ( status == PROCAPI_NOPID || status == PROCAPI_PERM ) {
				break;
			} else {
				continue;
			}
		}

	        // format of /proc/self/stat is
	        // pid (ProcessName) State number number number number...
	        // The process name can have spaces in it, so read the
	        // whole line in with fgets, whack the spaces, then 
	        // parse again with sprintf
	        char line[512];
		if (fgets(line, 512, fp) == NULL) {
			// couldn't read the right number of entries.
			status = PROCAPI_UNSPECIFIED;
			dprintf( D_ALWAYS, 
				"ProcAPI: Read error on %s: errno (%d): %s\n", 
				 path, errno,  strerror(errno));

			// don't leak for the next attempt;
			fclose( fp );
			fp = NULL;

			// try again
			continue;
		}

		char *rparen = strrchr(line, ')');
		char *lparen = strchr(line, '(');
		if (lparen && rparen && lparen < rparen) {
			while (lparen != rparen) {
				if (*lparen == ' ') {
					*lparen = '_';
				}
				lparen++;
			}
		}

			// fill the raw structure from the proc file
			// ensure I read the right number of arguments....
		if ( sscanf( line, "%d %s %c %d "
			"%ld %ld %ld %ld "
			"%lu %lu %lu %lu %lu "
			"%ld %ld %ld %ld %ld %ld "
			"%lu %lu %llu %llu %lu %lu %lu %lu %lu %lu %lu "
			"%ld %ld %ld %ld %lu",
			&procRaw.pid, s, &c, &procRaw.ppid, 
			&i, &i, &i, &i, 
			&procRaw.proc_flags, &procRaw.minfault, &u, &procRaw.majfault, &u, 
			&procRaw.user_time_1, &procRaw.sys_time_1, &i, &i, &i, &i, 
			&u, &u, &procRaw.creation_time, &imgsize_bytes, &procRaw.rssize, &u, &u, &u, 
			&u, &u, &u, &i, &i, &i, &i, &u ) != 35 )
		{
			// couldn't read the right number of entries.
			status = PROCAPI_UNSPECIFIED;
			dprintf( D_ALWAYS, 
				"ProcAPI: Unexpected short scan on %s, (%s) errno: %d.\n", 
				 path, line, errno );

			// don't leak for the next attempt;
			fclose( fp );
			fp = NULL;

			// try again
			continue;
		}

		// covert imgsize_bytes to k
		procRaw.imgsize = imgsize_bytes/1024;

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
	fd = safe_open_wrapper_follow(path, O_RDONLY);

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
					EXCEPT( "Procapi::getProcInfo: Out of memory!");
				}
			} else {
				env_buffer = (char*)realloc(env_buffer, read_size * multiplier);
				if ( env_buffer == NULL ) {
					EXCEPT( "Procapi::getProcInfo: Out of memory!");
				}
				multiplier++;
			}

			bytes_read = full_read(fd, env_buffer+bytes_read_so_far, read_size);
			// We have seen cases where read() returns a value in the 1GB
			// range. Retrying after a lseek() and/or reopening the file
			// gave the same result. So just give up in that case.
			if ( bytes_read < 0 || bytes_read > read_size ) {
				close( fd );
				free( env_buffer );
				return PROCAPI_SUCCESS;
			}

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
			EXCEPT( "Procapi::getProcInfo: Out of memory!");
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
					"environment variables in pid %u. Programmer Error.",
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
		if( (fp = safe_fopen_wrapper_follow("/proc/uptime","r")) ) {
			double uptime=0;
			double dummy=0;
			char *r = fgets( s, 256, fp );
			if (r && sscanf( s, "%lf %lf", &uptime, &dummy ) >= 1) {
				// uptime is number of seconds since boottime
				// convert to nearest time stamp
				uptime_boottime = (unsigned long)(now - uptime + 0.5);
			}
			fclose( fp );
		}

		// get stat_boottime
		if( (fp = safe_fopen_wrapper_follow("/proc/stat", "r")) ) {
			char * r = fgets( s, 256, fp );
			while( r && strstr(s, "btime") == NULL ) {
				r = fgets( s, 256, fp );
			}
			(void) sscanf( s, "%s %lu", junk, &stat_boottime );
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

#elif defined(Darwin)

int
ProcAPI::getProcInfo( pid_t pid, piPTR& pi, int &status ) 
{

		// This *could* allocate memory and make pi point to it if pi == NULL.
		// It is up to the caller to get rid of it.
	initpi(pi);

	return getProcInfo_impl(pid, pi, status);
}

int
ProcAPI::getProcInfo_impl( pid_t pid, piPTR& pi, int &status )
{
		// get the raw system process data
	procInfoRaw procRaw;
		// Some internal callers can pre-fill the data that's obtained
		// from sysctl(). They will indicate this by setting pi->pid to
		// match the pid argument.
		// If procRaw.pid matches the pid argument, then
		// getProcInfoRaw() will not call sysctl() and leave those fields
		// unfilled.
	procRaw.pid = pi->pid;
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

		// compute the cpu time
	double cpu_time = procRaw.user_time_1 + procRaw.sys_time_1;

		// copy the remainder of the fields
	pi->user_time = procRaw.user_time_1;
	pi->sys_time = procRaw.sys_time_1;
		// Some internal callers can pre-fill this data that's obtained
		// from sysctl(). They will indicate this by setting pi->pid to
		// match the pid argument. In that case, preserve the values that
		// are already present.
	if ( pi->pid != pid ) {
		pi->creation_time = procRaw.creation_time;
		pi->birthday = procRaw.creation_time;
		pi->pid = procRaw.pid;
		pi->ppid = procRaw.ppid;
		pi->owner = procRaw.owner;
	}
	
		// compute the age
		// Use creation_time from pi, since getProcInfoRaw() may not
		// have filled out procRaw.creation_time.
	pi->age = procRaw.sample_time - pi->creation_time;

		// convert the number of page faults into a rate
	do_usage_sampling(pi, cpu_time, procRaw.majfault, procRaw.minfault);

	return PROCAPI_SUCCESS;
}

size_t ProcAPI::getBasicUsage(pid_t pid, double * puser_time, double * psys_time)
{
	int status;
	procInfoRaw procRaw;
	int retVal = ProcAPI::getProcInfoRaw(pid, procRaw, status);
	if (retVal != 0){
		//on failure, set everything to 0.
		initProcInfoRaw(procRaw);
	}

	if (puser_time) {
		*puser_time = procRaw.user_time_1;
	}
	if (psys_time) {
		*psys_time = procRaw.sys_time_1;
	}
	return (size_t)procRaw.imgsize;
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
	struct kinfo_proc kp;
	size_t bufSize = sizeof(struct kinfo_proc);
	bool call_sysctl = pid != procRaw.pid;

		// assume success
	status = PROCAPI_OK;

		// clear memory for procRaw
	initProcInfoRaw(procRaw);

		// set the sample time
	procRaw.sample_time = secsSinceEpoch();
	
		/* Collect the data from the system */

	if ( call_sysctl ) {
			// First, let's get the BSD task info for this stucture. This
			// will tell us things like the pid, ppid, etc.
		mib[0] = CTL_KERN;
		mib[1] = KERN_PROC;
		mib[2] = KERN_PROC_PID;
		mib[3] = pid;
		if (sysctl(mib, 4, &kp, &bufSize, NULL, 0) < 0) {
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
				"ProcAPI: sysctl() on pid %d failed with %d(%s)\n",
				pid, errno, strerror(errno) );

			return PROCAPI_FAILURE;
		}
		if ( bufSize == 0 ) {
			status = PROCAPI_NOPID;
			dprintf( D_FULLDEBUG,
				"ProcAPI: sysctl() on pid %d returned no data\n",
				pid );
			return PROCAPI_FAILURE;
		}
	}

	// figure out the image,rss size and the sys/usr time for the process.

	struct rusage_info_v0 ru;
	if ( proc_pid_rusage( pid, RUSAGE_INFO_V0,
						  reinterpret_cast<rusage_info_t*>(&ru) ) < 0 ) {
		// If this call fails, set all values to zero.
		// Failure is unexpected if we're root (since sysctl() succeeded),
		// so that's worth a minor log entry.
		if (getuid() == 0 || geteuid() == 0) {
			dprintf( D_FULLDEBUG,
				"ProcAPI: proc_pid_rusage() on pid %d failed with %d(%s)\n",
				pid, errno, strerror(errno) );
		}
		memset(&ru, 0, sizeof(ru));
	}

	// fill in the values we got from the kernel
	// CPU times are given in nanoseconds.
	procRaw.imgsize = (unsigned long)ru.ri_phys_footprint;
	procRaw.rssize = ru.ri_resident_size;
	procRaw.user_time_1 = ru.ri_user_time /  1'000'000'000;
	procRaw.user_time_2 = ru.ri_user_time %  1'000'000'000;
	procRaw.sys_time_1 = ru.ri_system_time / 1'000'000'000;
	procRaw.sys_time_2 = ru.ri_system_time % 1'000'000'000;

	// add in the rest
	procRaw.pid = pid;
	if ( call_sysctl ) {
		procRaw.creation_time = kp.kp_proc.p_starttime.tv_sec;
		procRaw.ppid = kp.kp_eproc.e_ppid;
		procRaw.owner = kp.kp_eproc.e_pcred.p_ruid;
	}

	// We don't know the page faults
	procRaw.majfault = 0;
	procRaw.minfault = 0;

	// success
	return PROCAPI_SUCCESS;
}

int
ProcAPI::buildProcInfoList(pid_t /*BOLOpid*/)
{
	int mib[4];
	struct kinfo_proc *kp = NULL;
	size_t bufSize = 0;
	int nentries;
	int rc = -1;
	int ntries = 5;

	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC;
	mib[2] = KERN_PROC_ALL;
	mib[3] = 0;

	do {
		ntries--;
		//
		// Returns back the size of the kinfo_proc struct
		//
		if (sysctl(mib, 4, NULL, &bufSize, NULL, 0) < 0) {
			dprintf(D_ALWAYS, "ProcAPI: Failed to get list of pids: %s\n",
					strerror(errno));
			free( kp );
			return PROCAPI_FAILURE;
		}

		ASSERT( bufSize );
		kp = (struct kinfo_proc *)realloc(kp, bufSize);

		rc = sysctl(mib, 4, kp, &bufSize, NULL, 0);
	} while ( ntries >= 0 && ( ( rc == -1 && errno == ENOMEM ) || ( rc == 0 && bufSize == 0 ) ) );

	if ( rc == -1 || bufSize == 0 ) {
		dprintf(D_ALWAYS, "ProcAPI: Failed to get list of pids: %s\n",
				strerror(errno));
		free(kp);
		return PROCAPI_FAILURE;
	}

	nentries = bufSize / sizeof(struct kinfo_proc);

	piPTR current;
	piPTR temp;
	int status;

		// make a header node for ease of list construction:
	deallocAllProcInfos();
	allProcInfos = new procInfo;
	current = allProcInfos;
	current->next = NULL;

	temp = NULL;
	for(int i = 0; i < nentries; i++) {
			// Pid 0 is not a real process. It represents the kernel.
		if ( kp[i].kp_proc.p_pid == 0 ) {
			continue;
		}
		initpi(temp);
		temp->pid = kp[i].kp_proc.p_pid;
		temp->ppid = kp[i].kp_eproc.e_ppid;
		temp->owner = kp[i].kp_eproc.e_pcred.p_ruid;
		temp->creation_time = kp[i].kp_proc.p_starttime.tv_sec;
		temp->birthday = kp[i].kp_proc.p_starttime.tv_sec;
		if( getProcInfo_impl(temp->pid, temp, status) == PROCAPI_SUCCESS) {
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

	free(kp);

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

size_t ProcAPI::getBasicUsage(pid_t pid, double * puser_time, double * psys_time)
{
	int status;
	procInfoRaw procRaw;
	int retVal = ProcAPI::getProcInfoRaw(pid, procRaw, status);
	if (retVal != 0){
		//on failure, set everything to 0.
		initProcInfoRaw(procRaw);
	}

	if (puser_time) {
		*puser_time = procRaw.user_time_1;
	}
	if (psys_time) {
		*psys_time = procRaw.sys_time_1;
	}
	return (size_t)procRaw.imgsize;
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
		EXCEPT("ProcAPI: getProcInfo() Out of memory!");
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
	if( (fp = safe_fopen_wrapper_follow( path, "r" )) == NULL ) {
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

size_t ProcAPI::getBasicUsage(pid_t pid, double * puser_time, double * psys_time)
{
	if (pid == getpid()) {
		if (puser_time || psys_time) {
			UINT64 ntCreate=0, ntExit=0, ntSys=0, ntUser=0; // nanotime. tick rate of 100 nanosec.
			if ( ! GetProcessTimes(GetCurrentProcess(),
									(FILETIME*)&ntCreate, (FILETIME*)&ntExit,
									(FILETIME*)&ntSys, (FILETIME*)&ntUser)) {
				ntSys = ntUser = 0;
			}
			if (puser_time) {
				*puser_time = (double)ntUser / (double)(1000*1000*10); // convert to seconds
			}
			if (psys_time) {
				*psys_time = (double)ntSys / (double)(1000*1000*10); // convert to seconds
			}
		}

		PROCESS_MEMORY_COUNTERS_EX mem;
		ZeroMemory(&mem, sizeof(mem));
		if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&mem, sizeof(mem))) {
			return mem.PrivateUsage;
		}
	}

	int status;
	procInfoRaw procRaw;
	int retVal = ProcAPI::getProcInfoRaw(pid, procRaw, status);
	if (retVal != 0){
		if (puser_time) *puser_time = 0.0;
		if (psys_time) *psys_time = 0.0;
		return 0;
	}

	if (puser_time) {
		*puser_time = (double)procRaw.user_time / procRaw.object_frequency;
	}
	if (psys_time) {
		*psys_time = (double)procRaw.sys_time / procRaw.object_frequency;
	}
	return (size_t)procRaw.imgsize;
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
	DWORD last_error = 0;
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
    ULONG_PTR ctrblk;

    pThisObject = firstObject (pDataBlock);
  
    procRaw.sample_time = pThisObject->PerfTime.QuadPart;
    procRaw.object_frequency = pThisObject->PerfFreq.QuadPart;

    if( !offsets ) {      // If we haven't yet gotten the offsets, grab 'em.
        grabOffsets( pThisObject );
        ASSERT( offsets );
    }
        // at this point we're all set to march through the data block to find
        // the instance with the pid we want.  

    pThisInstance = firstInstance(pThisObject);
    instanceNum = 0;
    bool found = false;
    pid_t thispid;

    while( (!found) && (instanceNum < pThisObject->NumInstances) ) {
        ctrblk = ((ULONG_PTR)pThisInstance) + pThisInstance->ByteLength;
        thispid = (pid_t) *((long*)(ctrblk + offsets->procid));
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

    procRaw.pid       = (pid_t) *((long*)(ctrblk + offsets->procid  ));
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
ProcAPI::buildProcInfoList(pid_t /*BOLOpid*/)
{
    double begin = qpcBegin();

	deallocAllProcInfos();

	if (GetProcessPerfData() != PROCAPI_SUCCESS) {
		return PROCAPI_FAILURE;
	}

    ++cGetProcInfoList;
    ++cGetProcInfoListReg;
    sGetProcInfoListReg += qpcDeltaSec(begin);

	// If we haven't yet gotten the offsets, grab 'em.
	//
	PPERF_OBJECT_TYPE pThisObject = firstObject(pDataBlock);
    if( !offsets ) {
        grabOffsets( pThisObject );
        ASSERT( offsets );
	}
	PPERF_INSTANCE_DEFINITION pThisInstance = firstInstance(pThisObject);

	__int64 sampleObjectTime = pThisObject->PerfTime.QuadPart;
	__int64 objectFrequency  = pThisObject->PerfFreq.QuadPart;

    // loop through each instance in data
	//
	ULONG_PTR ctrblk;
    int instanceNum = 0;
    while( instanceNum < pThisObject->NumInstances ) {

		procInfo* pi;

        ctrblk = ((ULONG_PTR)pThisInstance) + pThisInstance->ByteLength;

		LARGE_INTEGER elt = *(LARGE_INTEGER*)(ctrblk + offsets->elapsed);
        LARGE_INTEGER pt = *(LARGE_INTEGER*)(ctrblk + offsets->pctcpu);
        LARGE_INTEGER ut = *(LARGE_INTEGER*)(ctrblk + offsets->utime);
        LARGE_INTEGER st = *(LARGE_INTEGER*)(ctrblk + offsets->stime);

		pi = NULL;
		initpi(pi);

        double iter_start = qpcBegin();

        pi->pid = *(pid_t*)(ctrblk + offsets->procid);
		pi->ppid = ntSysInfo.GetParentPID(pi->pid);

        ++cGetProcInfoListPid;
        sGetProcInfoListPid += qpcDeltaSec(iter_start);

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

        iter_start = qpcBegin();

                        /* We figure out the cpu usage (a total counter, not a
                           percent!) and the total page faults here. */
		double cpu = LI_to_double( pt ) / objectFrequency;
		long faults = *(long*)(ctrblk + offsets->faults);

		/* figure out the %cpu and %faults */
		do_usage_sampling (pi, cpu, faults, 0, sampleObjectTime / objectFrequency);

        ++cGetProcInfoListCPU;
        sGetProcInfoListCPU += qpcDeltaSec(iter_start);

		pi->next = allProcInfos;
		allProcInfos = pi;

		pThisInstance = nextInstance( pThisInstance );
		instanceNum++;
	}

    sGetProcInfoList += qpcDeltaSec(begin);
	return PROCAPI_SUCCESS;
}

int
ProcAPI::GetProcessPerfData()
{
	DWORD dwStatus;  // return status of fn. calls

        // '2' is the 'system' , '230' is 'process'  
        // I hope these numbers don't change over time... 
    dwStatus = GetSystemPerfData (const_cast<char *>(TEXT("230")));

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
	if ( now - last_garbage_collection_time > 3600 ) {
		last_garbage_collection_time = now;
			// first delete anything still flagged as garbage
		auto it = procHash.begin();
		while (it != procHash.end()) {
			if ( it->second.garbage == true ) {
				// it is still flagged as garbage; delete it
				it = procHash.erase(it);
			} else {
				// it is not still flagged as garbarge; so do 
				// not delete it, but instead reset the garbage
				// flag to true.  It will be cleared when/if we
				// perform a lookup on this node.
				it->second.garbage = true;
				it++;
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
	auto it = procHash.find(pi->pid);
	if (it != procHash.end()) {

		phn = &it->second;

		/* Allow 2 seconds "slack" on creation time, like we do in
		   ProcFamily, since (at least on Linux) the value can
		   oscillate. Jim B. 11/29/00 */

		long birth = phn->creation_time - pi->creation_time;
		if (-2 > birth || birth > 2) {
			/* must be a different process associated with this, remove it. */
			procHash.erase(it);
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

        auto [it, success] = procHash.emplace(pi->pid, procHashNode());
        ASSERT(success);
        phn = &it->second;
    }

		// put new vals back into hashtable
	phn->lasttime = now;
	phn->oldtime  = ustime;   // store raw data for next call...
	phn->oldminf  = nowminf;  //  ""
	phn->oldmajf  = nowmajf;  //  ""
	phn->oldusage = pi->cpuusage;  // Also store results in case the
	phn->minfaultrate = pi->minfault;   // next sample is < 1 sec
	phn->majfaultrate = pi->majfault;   // from now.
	phn->creation_time = pi->creation_time;

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

}

procInfo*
ProcAPI::getProcInfoList(pid_t BOLOpid)
{
	if (buildProcInfoList(BOLOpid) != PROCAPI_SUCCESS) {
		dprintf(D_ALWAYS,
		        "ProcAPI: error retrieving list of process data\n");
		deallocAllProcInfos();
	}

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
#if HAVE_PSS
    pi->pssize = 0;
	pi->pssize_available = false;
#endif

	pidenvid_init(&pi->penvid);
}

void
ProcAPI::initProcInfoRaw(procInfoRaw& procRaw){
	memset(&procRaw, 0, sizeof(procInfoRaw));
}

#ifndef WIN32

/* Wonderfully enough, this works for all OS'es except OS X and HPUX. 
   OS X has it's own version, HP-UX never calls it.
   This function opens
   up the /proc directory and reads the pids of all the processes in the
   system.  This information is put into a list of pids that is pointed
   to by pidList, a private data member of ProcAPI.  
 */

#if !defined(Darwin) && !defined(CONDOR_FREEBSD)

#include <fstream>

int
build_pid_list( std::vector<pid_t> & newPidList, pid_t BOLOpid = 0) {
	//
	// If you mount /proc with the option hidepid=2, then PID 1 isn't
	// necessarily listed in /proc.
	//
	// See condor_utils/filesystem_remap.cpp for details.
	//
	static bool hidepid = true;
	static bool checked_proc_mountinfo = false;
	if(! checked_proc_mountinfo) {
		std::string line;
		std::ifstream file("/proc/self/mountinfo");
		if( file.good() ) {
			while(! file.eof()) {
				getline(file, line);
				if(! file.good()) { break; }

				std::string token;
				std::istringstream is(line);
				getline(is, token, ' '); // mount ID
				getline(is, token, ' '); // parent ID
				getline(is, token, ' '); // major:minor
				getline(is, token, ' '); // root
				getline(is, token, ' '); // mount point
				std::string mount_point = token;
				getline(is, token, ' '); // mount options

				// Any number of optional fields followed by a '-'.
				do {
					getline(is, token, ' ');
				} while( token != "-" );

				getline(is, token, ' '); // filesystem type
				getline(is, token, ' '); // mount source
				getline(is, token, ' '); // per-superblock options
				std::string ps_options = token;

				bool found_proc = false;
				bool found_hidepid = false;
				if( mount_point == "/proc" ) {
					found_proc = true;

					std::string option;
					std::istringstream psos(ps_options);
					while(! psos.eof()) {
						getline(psos, option, ',');
						if(! psos.fail()) {
							size_t pos = option.find("hidepid");
							if( pos == 0 ) {
								found_hidepid = true;
								try {
									std::string value = option.substr(7 + 1);
									int v = std::stoi(value);
									if( v <= 1 ) {
										dprintf( D_ALWAYS, "Found per-superblock option hidepid <= 1 for /proc, enabling check for PID 1.\n" );
										hidepid = false;
										break;
									}
								}
								catch( std::out_of_range & e ) { break; }
								catch( std::invalid_argument & e ) { break; }
							}
						}
					}

					// The default mount option for hidepid is 0; indeed, if you
					// explicitly specify hidepid=0 in the mount command, it
					// won't appear in /proc/self/mountinfo.
					if( found_proc && ! found_hidepid ) {
						dprintf( D_ALWAYS, "/proc was mounted without hidepid, assuming default of 0.\n" );
						hidepid = false;
					}

					// We found `proc`, we can quit reading now.
					break;
				}
			}

			file.close();
		}

		checked_proc_mountinfo = true;
	}

	pid_t my_pid = getpid();
	pid_t my_ppid = getppid();

	bool saw_pid1 = false;
	bool saw_ppid = false;
	bool saw_pid = false;
	bool saw_bolo_pid = false;

	condor_DIR * dirp = condor_opendir("/proc");
	if( dirp == NULL ) {
		dprintf( D_ALWAYS, "ProcAPI: opendir('/proc') failed (%d): %s\n", errno, strerror(errno) );
		return -1;
	}

	newPidList.clear();

	int total_entries = 0;
	int pid_entries = 0;

	// NOTE: this will use readdir64() when available to avoid
	// skipping over directories with an inode value that
	// doesn't happen to fit in the 32-bit ino_t
	condor_dirent *direntp;
	errno = 0;
	while( (direntp = condor_readdir(dirp)) != NULL ) {
		++total_entries;
		if( isdigit(direntp->d_name[0]) ) {
			pid_t the_pid = (pid_t)atol(direntp->d_name);
			newPidList.push_back(the_pid);
			++pid_entries;

			if( the_pid == 1 ) { saw_pid1 = true; }
			if( the_pid == my_ppid ) { saw_ppid  = true; }
			if( the_pid == my_pid ) { saw_pid = true; }
			if( the_pid == BOLOpid ) { saw_bolo_pid = true; }
		}
	}
	if( errno != 0 ) {
		dprintf(D_ALWAYS, "ProcAPI: readdir() failed: errno %d (%s)\n",
			errno, strerror(errno));
		condor_closedir( dirp );
		return -2;
	}
	condor_closedir( dirp );

	dprintf(D_FULLDEBUG, "ProcAPI: read %d pid entries out of %d total entries in /proc\n", pid_entries, total_entries);

	if (saw_bolo_pid) {
		dprintf(D_FULLDEBUG, "As expected, we saw root of subfamily pid of %d\n", BOLOpid);
	} else {
		if (BOLOpid != 0) {
			dprintf(D_ALWAYS, "Warning, expected subfamily pid of %d was not found in /proc, adding to set of assumed alived pids\n", BOLOpid);
			newPidList.push_back(BOLOpid);
			++pid_entries;
		}
	}
	if( (hidepid || saw_pid1) && saw_ppid && saw_pid ) {
		return pid_entries;
	} else {
		return -3;
	}
}

int
ProcAPI::buildPidList(pid_t BOLOpid) {
	static bool retry = true;

	std::vector<pid_t> newPidList;
	int rv = build_pid_list(newPidList, BOLOpid);

    //
    // Based on analysis of our logs from December 2020 and the first
    // three weeks of 2021.  On the faulting machines, approximately
    // 1 read in 4337 results in 10% fewer PIDs than the previous read;
    // depending on the machine, as few as 2 in 26 or as many as 2 in 3
    // of these short reads result in a failure we (humans) noticed.
    //
    // Conversely, every observed failure had a read short by at least
    // 15%, so even with a high false positive rate, it's worth the low
    // overhead to use the 10% threshold.
    //
    double fraction = 0.90;
    char * fraction_str = getenv( "_CONDOR_PROCAPI_RETRY_FRACTION" );
    if( fraction_str ) {
        char * endptr = NULL;
        double d = strtod( fraction_str, & endptr );
        if( endptr != NULL && *endptr == '\0' ) {
            fraction = d;
        }
    }

    //
    // FIXME: this code doesn't ever reset the expected number of PIDs,
    // so the test suite (and possible others) gets stuck in a loop of
    // "bad" reads which continues to return increasingly-obsolete data
    // about the PIDs until something (e.g., registration of a job's PID
    // by the starter) fails.  So I'm disabling this test until we figure
    // out something saner to do.  Since this test was broken for months
    // without causing CHTC any problems, maybe we won't have to.
    //
    bool suddenly_too_many_fewer = false;
    if( rv >= 0 && rv < (int)(pidList.size() * fraction) ) {
        dprintf( D_ALWAYS, "PROCAPI_RETRY_FRACTION = %f means that the current read of %d is suddenly too much smaller than the previous read of %zu\n", fraction, rv, pidList.size() );
        // suddenly_too_many_fewer = true;
    }

	if( rv == -1 ) {
		return PROCAPI_FAILURE;
	} else if( rv == -2 ) {
		return PROCAPI_FAILURE;
	} else if( rv == -3 || suddenly_too_many_fewer ) {
		dprintf( D_ALWAYS, "ProcAPI: detected invalid read of /proc.\n" );

		std::stringstream buffer;

		for( auto pid : pidList ) {
			buffer << " " << pid;
		}
		dprintf( D_ALWAYS, "ProcAPI: previous PID list:%s\n",
			buffer.str().c_str() );

		for( auto pid: newPidList ) {
			buffer << " " << pid;
		}
		dprintf( D_ALWAYS, "ProcAPI: new PID list:%s\n",
			buffer.str().c_str() );

		if( retry ) {
			dprintf( D_ALWAYS, "ProcAPI: retrying.\n" );
			retry = false; rv = buildPidList(); retry = true;
			return rv;
		} else {
			dprintf( D_ALWAYS, "ProcAPI: giving up, retaining previous PID list.\n" );
			return PROCAPI_SUCCESS;
		}
	} else {
		pidList = newPidList;
		return PROCAPI_SUCCESS;
	}
}

#endif

#if defined(CONDOR_FREEBSD)
int
ProcAPI::buildPidList() {

	pidList.clear();

	int mib[4];
	struct kinfo_proc *kp = NULL;
	size_t bufSize = 0;
	int nentries;
	int rc = -1;
	int ntries = 5;

	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC;
	mib[2] = KERN_PROC_ALL;
	mib[3] = 0;
	if (sysctl(mib, 3, NULL, &bufSize, NULL, 0) < 0) {
		dprintf(D_ALWAYS, "ProcAPI: Failed to get list of pids: %s\n",
				strerror(errno));
		return PROCAPI_FAILURE;
	}	

	do {
		ntries--;
		kp = (struct kinfo_proc *)realloc(kp, bufSize);

		rc = sysctl(mib, 3, kp, &bufSize, NULL, 0);
	} while ( ntries >= 0 && rc == -1 && errno == ENOMEM );

	if ( rc == -1 ) {
		dprintf(D_ALWAYS, "ProcAPI: Failed to get list of pids: %s\n",
				strerror(errno));
		free(kp);
		return PROCAPI_FAILURE;
	}

	nentries = bufSize / sizeof(struct kinfo_proc);

	for(int i = 0; i < nentries; i++) {
#if defined(CONDOR_FREEBSD4)
		pidList.push_back((pid_t) kp[i].kp_proc.p_pid);
#else
		pidList.push_back(kp[i].ki_pid);
#endif
	}
    
	free(kp);

	return PROCAPI_SUCCESS;
}
#endif

#if !defined(WIN32) && !defined(DARWIN)
int
ProcAPI::buildProcInfoList(pid_t BOLOpid) {
  
	piPTR current;
	piPTR temp;
	int status;

	deallocAllProcInfos();

	if (buildPidList(BOLOpid) != PROCAPI_SUCCESS) {
		dprintf(D_ALWAYS, "ProcAPI: error retrieving list of processes\n");
		return PROCAPI_FAILURE;
	}

		// make a header node for ease of list construction:
	allProcInfos = new procInfo;
	current = allProcInfos;
	current->next = NULL;

	temp = NULL;
	for( pid_t thispid : pidList ) {
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
	
#if defined(HAVE_FSTAT64) && !defined(DARWIN)
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


#endif // not defined WIN32

void 
ProcAPI::deallocAllProcInfos() {

	freeProcInfoList(allProcInfos);
	allProcInfos = NULL;
}

size_t
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
	ASSERT( offsets );

    pThisCounter = firstCounter(pThisObject);	   // "% Processor Time"
//    printcounter ( stdout, pThisCounter );
    offsets->pctcpu = pThisCounter->CounterOffset; // % cpu
	if (pThisCounter->CounterSize != 8) {
		unexpected_counter_size("total CPU", pThisCounter->CounterSize, "8");
	}
    
    pThisCounter = nextCounter(pThisCounter); // "% User Time"
//    printcounter ( stdout, pThisCounter );
    offsets->utime = pThisCounter->CounterOffset;  // % user time
	if (pThisCounter->CounterSize != 8) {
		unexpected_counter_size("user CPU", pThisCounter->CounterSize, "8");
	}
  
    pThisCounter = nextCounter(pThisCounter); // "% Privileged Time"
//    printcounter ( stdout, pThisCounter );
    offsets->stime = pThisCounter->CounterOffset;  // % sys time
	if (pThisCounter->CounterSize != 8) {
		unexpected_counter_size("system CPU", pThisCounter->CounterSize, "8");
	}
  
    pThisCounter = nextCounter(pThisCounter);  // "Virtual Bytes Peak"
    pThisCounter = nextCounter(pThisCounter);  // "Virtual Bytes"
//    printcounter ( stdout, pThisCounter );
    offsets->imgsize = pThisCounter->CounterOffset;  // image size
	if (pThisCounter->CounterSize != 8) {
		unexpected_counter_size("image size", pThisCounter->CounterSize, "8");
	}
  
    pThisCounter = nextCounter(pThisCounter);  // "Page Faults/Sec"
//    printcounter ( stdout, pThisCounter );
    offsets->faults = pThisCounter->CounterOffset;   // page faults
	if (pThisCounter->CounterSize != 4) {
		unexpected_counter_size("page faults", pThisCounter->CounterSize, "4");
	}
    
    pThisCounter = nextCounter(pThisCounter);  // "Working Set Peak"
    offsets->rssize = pThisCounter->CounterOffset;   // working set peak 
	offsets->rssize_width = pThisCounter->CounterSize;
	if ((offsets->rssize_width != 4) && (offsets->rssize_width != 8)) {
		unexpected_counter_size("working set",
		                        pThisCounter->CounterSize,
		                        "4 or 8");
	}

//    printcounter ( stdout, pThisCounter );
	pThisCounter = nextCounter(pThisCounter); // "Working Set"
	pThisCounter = nextCounter(pThisCounter); // "Page File Bytes Peak"

    
    pThisCounter = nextCounter(pThisCounter); // "Page File Bytes"
    pThisCounter = nextCounter(pThisCounter); // "Private Bytes"
    pThisCounter = nextCounter(pThisCounter); // "Thread Count"
    pThisCounter = nextCounter(pThisCounter); // "Priority Base"
    pThisCounter = nextCounter(pThisCounter); // "Elapsed Time"
//    printcounter ( stdout, pThisCounter );
    offsets->elapsed = pThisCounter->CounterOffset;  // elapsed time (age)
	if (pThisCounter->CounterSize != 8) {
		unexpected_counter_size("elapsed time",
		                        pThisCounter->CounterSize,
		                        "8");
	}
    
    pThisCounter = nextCounter(pThisCounter);  // "ID Process"
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
double ProcAPI::LI_to_double ( LARGE_INTEGER & bigun ) {
  
  double ret;
  ret = (double) bigun.LowPart;
  ret += ( ((double) bigun.HighPart) * ( ((double)0xffffffff) + 1.0 ) ) ;
  return ret;
}

int ProcAPI::grabDataBlockStats(int & cA, int & cR, int & cQueries, double & sQueries, size_t & cbAlloc, size_t & cbData)
{
   cA = cAllocs;
   cR = cReallocs;
   cbAlloc = cbDataBlockAlloc;
   cbData  = cbDataBlockData;
   cQueries = cPerfDataQueries;
   sQueries = sPerfDataQueries;
   return cGetSystemPerfDataCalls;
}

double ProcAPI::qpcBegin()
{
   LARGE_INTEGER li;
   QueryPerformanceCounter(&li);
   return LI_to_double(li);
}

double ProcAPI::qpcDeltaSec(double dstart_ticks)
{
   LARGE_INTEGER li;
   QueryPerformanceCounter(&li);
   double dend_ticks = LI_to_double(li);
   QueryPerformanceFrequency(&li);
   double dfreq = LI_to_double(li);
   return (dend_ticks - dstart_ticks) / dfreq;
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
    ++cAllocs;
  }

  ++cGetSystemPerfDataCalls;

  while ( TRUE ) {
    Size = (UINT)_msize ( pDataBlock ); 
    
    cbDataBlockAlloc = Size;
    ++cPerfDataQueries;
    double begin = qpcBegin();

    lError = RegQueryValueEx ( HKEY_PERFORMANCE_DATA, pValue, 0, &Type, 
             (LPBYTE) pDataBlock, &Size );
    
    cbDataBlockData = Size;
    sPerfDataQueries += qpcDeltaSec(begin);

    // check for success & valid perf data bolck struct.
    
    if ( (!lError) && (Size>0) && 
         (pDataBlock)->Signature[0] == (WCHAR)'P' &&
         (pDataBlock)->Signature[1] == (WCHAR)'E' &&
         (pDataBlock)->Signature[2] == (WCHAR)'R' &&
         (pDataBlock)->Signature[3] == (WCHAR)'F' )
      return ERROR_SUCCESS;
    
    // if buffer not big enough, reallocate and try again.
    
    if ( lError == ERROR_MORE_DATA ) {
      void * pvNew = realloc ( pDataBlock, _msize (pDataBlock ) + EXTEND_SIZE );
      if ( ! pvNew) 
        return lError;
      pDataBlock = (PPERF_DATA_BLOCK) pvNew;
      ++cReallocs;
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
	FILE* fp = safe_fopen_wrapper_follow("/proc/uptime", "r");
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
		fclose(fp);
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
ProcAPI::confirmProcessId(ProcessId& /*procId*/, int& status){
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
