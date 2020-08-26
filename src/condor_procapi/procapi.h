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

#ifndef PROCAPI_H
#define PROCAPI_H

/* The following written by Mike Yoder */
//@author Mike Yoder

/* Here are the necessary #includes */
#include "condor_common.h"
#include "condor_system.h"
#include "condor_pidenvid.h"
#include "processid.h"
#include "HashTable.h"
#include "extArray.h"

#ifndef WIN32 // all the below is for UNIX

#include <dirent.h>        // get /proc entries (directory stuff)
#include <sys/types.h>     // various types needed.
#include <time.h>          // use of time() for process age. 

#ifdef Darwin
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <mach/bootstrap.h>
#include <mach/mach_error.h>
#include <mach/mach_types.h>
#endif

#ifdef CONDOR_FREEBSD
#include <sys/types.h>
#include <sys/sysctl.h>
#include <errno.h>
#include <sys/procfs.h>
#include <sys/proc.h>
#include <sys/user.h>
#endif 

#else // It's WIN32...
// Warning: WIN32 stuff below.

typedef DWORD pid_t;

#define UNICODE 1
#define _UNICODE 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#include <windows.h>
#include <winperf.h>
#include <tchar.h>

#define INITIAL_SIZE    40960L    // init. size for getting pDataBlock
#define EXTEND_SIZE	     4096L    // incremental addition to pDataBlock

//LPTSTR is a point to a null-terminated windows or Unicode string.
//TEXT() basically puts a string into unicode.
// Here are some Windows specific strings.
const LPTSTR NamesKey = 
      TEXT("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Perflib");
const LPTSTR DefaultLangId = TEXT("009");  //english!
const LPTSTR Counters = TEXT("Counters");
const LPTSTR Help = TEXT("Help");
const LPTSTR LastHelp = TEXT("Last Help");
const LPTSTR LastCounter = TEXT("Last Counter");
const LPTSTR Slash = TEXT("\\");

struct Offset {       // There will be one instance of this structure in
	DWORD imgsize;      // the ProcAPI class - it will hold the offsets of
	DWORD rssize;       // the data that reside in the pDataBlock
	DWORD faults;
	DWORD pctcpu;       // this info is gotten by grabOffsets()
	DWORD utime;
	DWORD stime;        // this structure additionally holds data width
	DWORD elapsed;      // info for counters for which it can vary
	DWORD procid;
	int rssize_width;
};


#endif // WIN32

// return values of different procapi calls
enum {
	// general success
	PROCAPI_SUCCESS,

	// general failure
	PROCAPI_FAILURE
};

// status about various calls to procapi.
enum {
	// for when everything worked as desired
	PROCAPI_OK,

	// when you ask procapi for a pid family, and absolutely nothing is found
	PROCAPI_FAMILY_NONE,
	
	// when the daddy pid is included in the returned family
	PROCAPI_FAMILY_ALL,

	// when the daddy pid is NOT found, but others known to be its child are.
	PROCAPI_FAMILY_SOME,

	// failure due to nonexistance of pid
	PROCAPI_NOPID,

	// failure due to permissions
	PROCAPI_PERM,

	// sometimes a kernel simply screws up and gives us back garbage
	PROCAPI_GARBLED,

	// an error happened, but we didn't specify exactly what it was
	PROCAPI_UNSPECIFIED,

		// the process is definitely alive
	PROCAPI_ALIVE,

		// the process is definitely dead
	PROCAPI_DEAD,
	
		// it is uncertain whether the process is 
		// alive or dead
	PROCAPI_UNCERTAIN
};

/** This type serves to hold an opaque value representing a process's time
    of creation. It is platform-dependent since it holds whatever value the
    kernel gives us AS IS, so we can avoid rounding effects when using these
    values in comparisons to see if two processes share a birthday.
*/
#if defined(WIN32)
typedef __int64 birthday_t;
#define PROCAPI_BIRTHDAY_FORMAT "%I64d"
#elif defined(LINUX)
typedef unsigned long long birthday_t;
#define PROCAPI_BIRTHDAY_FORMAT "%llu"
#else
typedef long birthday_t;
#define PROCAPI_BIRTHDAY_FORMAT "%ld"
#endif

/** This is the structure that is returned from the getProcInfo() 
    member of ProcAPI.  It is returned all at once so that multiple
    calls don't have to be made.  All OS's support the given information,
    WITH THE EXCEPTION OF MAJ/MIN FAULTS:
    <ul>
     <li> Linux returns a reasonable-looking number.
    </ul>
    
    In the case of a 'family' of pids ( given a pid, return info on that
    pid + all its children ), the values returned are a sum of the process'
    values.  The exception to this is 'age', which is the wall clock age of
    the oldest process.
*/
struct procInfo {

  /// the image size (aka virtual memory), in k.
  unsigned long imgsize;
  /// the resident set size, in k.  on WinNT, it is peak working set size.
  unsigned long rssize;       

  /** The proportional set size (PSS) is the image size in k divided
      by number of processes sharing each page.  If pssize information
      is not available, pssize_available is false; o.w. true. */
#if HAVE_PSS
  unsigned long pssize;
  bool pssize_available;
#endif

  /** The number of minor page faults generated per second.  
      The definition of "minor" is "those which have not required 
      loading a memory page from disk"  Therefore, the page was
      still floating around somewhere in the free list.  */
  unsigned long minfault;

  /** The number of major page faults generated per second.
      The definition of "major" is "those which require loading
      a memory page from disk". */
  unsigned long majfault;

  /** percent of recent cpu time used by this process.  To maintain consistency
      between systems, this value must be sampled over time by hand.
      Here's how it works:  If this is the first reference to
      a pid, that pid's %cpu usage until that time is returned.  If the pid
      has been looked at before, the %cpu since the last reference is returned.
      So, if you look at a process every 10 seconds, you get the %cpu for
      those previous 10 seconds.<p>
  
      Note that this number is a PERCENT, i.e. range of 100.0 - 0.0, on
      a single processor machine.<p>
      
      On multiple processor machines, it works the same way.  However, say 
      you have a process with 2 threads.  If both of these threads are cpu 
      bound (and they each get their own processor), then you'll have a 
      usage of 200%.  For single threaded tasks, it's the same as above.

  */
  double cpuusage;

  /// time (secs) in user mode
  long user_time;
  /// time (secs) in system mode 
  long sys_time;
  /// seconds (wall clock) since process creation.
  long age; 

  /// this pid
  pid_t pid;
  /// parents pid.
  pid_t ppid;

  /// The time of birth of this pid
  long creation_time;

  /// The time of birth in system-specific units (e.g. Linux returns
  /// this in jiffies since boot time) This is useful since
  /// (hopefully) it is not subject to wobble caused by converting it
  /// to different units and thus can be used for accurate process
  /// birthday comparisons.
  birthday_t birthday;

  /// pointer to next procInfo, if one exists.
  struct procInfo *next;

#if !defined(WIN32)
  // the owner of this process
  uid_t owner;
#endif

  // Any ancestor process identification environment variables.
  // This is always initialzed to something, but might not be filled
  // in due to complexities with the OS.
  PidEnvID penvid;
};

/// piPTR is typedef'ed as a pointer to a procInfo structure.
typedef struct procInfo * piPTR;

/*
  Special structure for holding the raw, unchecked, unconverted,
  values returned by the OS when inquiring about a process.
  The range, units and in some cases even the type of each field 
  are determined by the OS.
*/
typedef struct procInfoRaw{

		// Virtual size and working set size
		// are reported as 64-bit quantities
		// on Windows
#ifndef WIN32
	unsigned long imgsize;
	unsigned long rssize;
#else
	__int64 imgsize;
	__int64 rssize;
#endif

#if HAVE_PSS
	unsigned long pssize;
	bool pssize_available;
#endif

	long minfault;
	long majfault;
	pid_t pid;
	pid_t ppid;
#if !defined(WIN32)
	uid_t owner;
#endif
	
		// Times are different on Windows
#ifndef WIN32
		  // some systems return these times
		  // in a combination of 2 units
		  // *_1 is always the larger units
	long user_time_1;
	long user_time_2;
	long sys_time_1;
	long sys_time_2;
	birthday_t creation_time;
	long sample_time;
#endif // not defined WIN32

// Windows does it different
#ifdef WIN32 
	__int64 user_time;
	__int64 sys_time;
	__int64 creation_time;
	__int64 sample_time;
	__int64 object_frequency;
	__int64 cpu_time;
#endif //WIN32

// special process flags for Linux
#ifdef LINUX
	  unsigned long proc_flags;
#endif //LINUX
}procInfoRaw;

/** procHashNode is used to hold information in the hashtable.  It is used
    to save the state of certain things that need to be sampled over time.
    For instance, the number of page faults is always given as a number 
    for the lifetime of a process.  I want to save this number and the
    time, and convert it into a page fault per second rate.  The other state
    want to save is the cpu uage times, for % cpu usage.
*/

struct procHashNode {
  /// Ctor
  procHashNode();
  /// the last time (secs) this data was retrieved.
  double lasttime;
  /// old cpu usage number (raw, not percent)
  double oldtime;
  /// the old value for cpu usage (the actual percent)
  double oldusage;
  /// the old value for minor page faults. (This is a raw # of total faults)
  long oldminf;
  /// the old value for major page faults.
  long oldmajf;
  /// the major page fault rate (per sec).  This is the last value sampled.
  long majfaultrate;
  /// the minor page fault rate.  Last value sampled.
  long minfaultrate;
  /// The "time of birth" of this process (in sec)
  long creation_time;
  /** A pretty good band... no, really, this flag is used by a simple garbage collection sheme
	  which controls when procHashNodes should be deleted.  First we set garbage to true.  
	  Then everytime we access this procHashNode we set it to false.  After an hour has passed,
	  any node which still has garbage set to true is deleted. */
  bool garbage;
};

/** pidHashFunc() is the hashing function used by ProcAPI for its
 *  HashTable of processes. Other code may want to use it for their
 *  own pid-keyed HashTables. The condor_procd does.
 */
size_t pidHashFunc ( const pid_t& pid );

/** The ProcAPI class returns information about running processes.  It is
    possible to get information for:
    <ul>
     <li> One pid.
     <li> A set of pids.
     <li> A pid and all of that pid's descendants.
    </ul>
    When you ask for information, a procInfo structure is returned to
    you containing the following:
    <ul>
     <li> Image size and resident set size.
     <li> Page faults (major & minor) per second.
     <li> % cpu usage.
     <li> Time spent in user and system modes.
     <li> The "wall clock" age of the process.
    </ul>
    Note that all of this information is returned at once.  This is because
    the operating system usually returns several of these values together
    in a structure, and it would be inefficient to repeatedly make system
    calls when only one call is really needed.  Also, 8 values * 3 types
    of things to get values for...equals 24 functions, which is way too many.
    <p>
    Note: memory for the procInfo structure can be allocated by the caller.
    If you pass a NULL pointer to one of the functions, space will be
    allocated for you.  Things will be bad if you pass a non-allocated, 
    non-NULL pointer into a member function.  Don't forget to deallocate
    the procInfo structure when done!

	<p>
	Note: Since there is no need to instantiate multiple copies of
	this class, the whole class is static, and should just never be
	instantiated at all.

    @author Mike Yoder
    @see procInfo
 */

class ProcAPI {
 public:

	 /** Destructor */
  ~ProcAPI();

  /** getProcInfo returns information about one process.  Information 
      can be retrieved on any process owned by the same user, and on some
      systems (Linux, Sol2.6) information can be gathered from all 
      processes.  

      @param pid The pid of the process you want info on.
      @param pi  A pointer to a procInfo structure.
	  @param status An indicator of the reason why a success or failure happened
      @return A 0 on success, and number less than 0 on failure.
      @see procInfo
  */
  static int getProcInfo ( pid_t pid, piPTR& pi, int &status );
#if defined(DARWIN)
  static int getProcInfo_impl ( pid_t pid, piPTR& pi, int &status );
#endif

  /** Feed this function a procInfo struct and it'll print it out for you. 

      @param pi A pointer to a procInfo structure.
      @return nothing
      @see procInfo
   */
  static void printProcInfo ( piPTR pi );
  static void printProcInfo ( FILE* fp, piPTR pi );

  /* returns a list of procInfo structures, for every process on the system.
     the list can be traversed using the "next" fields of the procInfo structures.

	@return a procInfo list representing all processes on the system
  */
  static procInfo* getProcInfoList();

  /* used to deallocate the memory for a list of procInfo structures

	@param The list to deallocate
  */
  static void freeProcInfoList(procInfo*);

  // these functions are needed by the old process-family tracking
  // logic, which is still used if "USE_PROCD = False" is set in
  // condor_config
  //
  static int getPidFamily( pid_t pid,
                           PidEnvID *penvid,
                           ExtArray<pid_t>& pidFamily,
                           int &status );
  static int getPidFamilyByLogin( const char *searchLogin,
                                  ExtArray<pid_t>& pidFamily );
  static int getProcSetInfo ( pid_t *pids,
                              int numpids,
                              piPTR& pi,
                              int &status );
  static int isinfamily ( pid_t *, int, PidEnvID*, piPTR ); 
#if defined(WIN32)
  static ExtArray<HANDLE> familyHandles;
  static void makeFamily( pid_t dadpid,
                          PidEnvID *penvid,
                          pid_t *allpids,
                          int numpids,
                          pid_t* &fampids,
                          int &famsize );
  static void getAllPids( pid_t* &pids, int &numpids );
  static void closeFamilyHandles();
  static int multiInfo( pid_t *pidlist, int numpids, piPTR &pi );
  static int isinlist( pid_t pid, pid_t *pidlist, int numpids );
#else
  static piPTR procFamily;
  static int buildFamily( pid_t,
                          PidEnvID *,
                          int & );
  static void deallocProcFamily();
  static int getNumProcs();
#endif

	  /*	
		Creates a ProcessId from the given pid.
		This process id can be used at a later time
		to determine whether a process is alive or dead.
		@param pid The pid of the process we want an ProcessId for
		@param pProcId an out parameter that is a pointer to a ProcessId.  
		This call initializes heap memory only in the case of success.
		@param status an out paramter that contains the failure code.
		@param precision_range is a parameter that specifies allowable precision error in the processId.  
		If it is NULL ProcAPI uses a system default.
		@return PROCAPI_SUCCESS or PROCAPI_FAILURE
	  */
  static int createProcessId(pid_t pid, ProcessId*& pProcId, int& status, int* precision_range = 0);
  
	  /*
		Determines whether the given process id is alive or dead.
		@param procId is the process id we are interested in
		@param status is an out parameter that contains PROCAPI_ALIVE, PROCAPI_DEAD, or PROCAPI_UNCERTAIN.
		@return PROCAPI_SUCCESS or PROCAPI_FAILURE
	  */
  static int isAlive(const ProcessId& procId, int& status);
  
	  /*
		Confirms that this process id is unique.  
  
		An entity MUST ensure that this process is not reaped for the
		amount of time returned by computeWaitTime() before confirming
		a this id.  Typically, the only entity that can confirm
		another process is its parent, since it is the only one that
		can ensure that the process is not reaped.  Although, any
		entity that has fail-safe mechanism of ensuring the process
		hasn't been reaped, outside of the functions provided in this
		class, can confirm it.  Think hard before you confirm a
		process you are not the parent of.
	  */
  static int confirmProcessId(ProcessId& procId, int& status);
  
#if HAVE_PSS
  /**
    * Calculate the Proportional Set Size, a Linux-specific metric
    * for memory usage taking into account memory sharing.
    *
    * This will operate directly on the procInfo reference; if this
    * function isn't called, PSS information will not be available through
    * the "normal" means.
    */
  static int getPSSInfo( pid_t pid, procInfo& procInfo, int &status );
#endif

  /**
    * Helper function for measuring code performance. get the cpu usage and 
    * memory usage of the given process.
    */
  static size_t getBasicUsage(pid_t pid, double * puser_time, double * psys_time=NULL);

 private:

  /** Default constructor.  It's private so that no one really
	  instantiates a ProcAPI object anywhere. */
  ProcAPI() {};		

  static void initpi ( piPTR& );                  // initialization of pi.

#if !defined(WIN32)
  static uid_t getFileOwner(int fd); // used to get process owner from /proc
#endif

  /**  
	Returns the raw OS stored data about the given pid.  Does no conversion
	of units, performs no sanity checking, merely gets the raw data
	from the OS.  This ensures that the values returned, if faulty, are
	faulty due to the OS rather than due to any conversions or sanity
	checks.

	@param pid The pid of the process you want info on.
	@param procRaw  A reference to a procInfoRaw structure.
	@param status An indicator of the reason why a success or failure happened
	@return A 0 on success, and number less than 0 on failure.
  */
  static int getProcInfoRaw(pid_t pid, procInfoRaw& procRaw, int &status);

	  /**
		 Clears the memory of a procInfoRaw struct.
		 @param procRaw A reference to a procInfoRaw structure.
	  
	  */
  static void initProcInfoRaw(procInfoRaw& procRaw);

  
	  /**
		 Generates a control time by which a process birthday can be
		 shifted in case of time shifting due to ntpd or the admin.
		 @param ctl_time out parameter that is filled with the control time
		 @param status out parameter that is filled with the failure code on failure.
		 @return A 0 on success, and a number less than 0 on failure
	   */
  static int generateControlTime(long& clt_time,  int& status);

	  /**
		 Generates a confirm time that confirms the process was alive
		 at said time.  The confirm time is in the same time units and time
		 space as the birthday.
		 @param confirm_time out parameter that is filled with the confirm time
		 @param status out parameter that is filled with the failure code on failure.
		 @return A 0 on success, and a number less than 0 on failure
	   */
  static int generateConfirmTime(long& confirm_time, int& status);

#ifdef LINUX
	  // extracts the environment from the system
	  // Currently only have a linux implementation
  static int fillProcInfoEnv(piPTR);
	  // updates the statically stored boottime variable if neccessary
	  // something similar probably belongs in sys_api
  static int checkBootTime(long now);
#endif //LINUX

  // works with the hashtable; finds cpuusage, maj/min page faults.
  static void do_usage_sampling( piPTR& pi, 
								 double ustime, 
								 long majf, 
								 long minf
#ifdef WIN32  /* Win32 also passes in the time. */
								 , double now
#endif
								 );

#if !defined(DARWIN) && !defined(WIN32)
  static int buildPidList();                      // just what it says
#endif
  static int buildProcInfoList();                 // ditto.
  static long secsSinceEpoch();                   // used for wall clock age
  static double convertTimeval ( struct timeval );// convert timeval to double
  static void deallocAllProcInfos();              // respective lists.

  public:
  static int getProcInfoListStats(double & sOverall, 
                                  int & cReg, double & sReg, 
                                  int & cPid, double & sPid, 
                                  int & cCPU, double & sCPU);
  private:
  static int cGetProcInfoList;
  static double sGetProcInfoList;
  static int cGetProcInfoListReg;
  static double sGetProcInfoListReg;
  static int cGetProcInfoListPid;
  static double sGetProcInfoListPid;
  static int cGetProcInfoListCPU;
  static double sGetProcInfoListCPU;

#ifdef WANT_STANDALONE_DEBUG
  static void sprog1 ( pid_t *, int, int );       // used by test1.
  static void sprog2 ( int, int );                // used by test2.
#endif  // of WANT_STANDALONE_DEBUG

#ifdef WIN32 // some windoze-specific thingies:

  static int GetProcessPerfData();
  static DWORD GetSystemPerfData ( LPTSTR pValue );
  static double LI_to_double ( LARGE_INTEGER & );
  static double qpcBegin();  // returns time in ticks
  static double qpcDeltaSec(double start_in_ticks); // returns delta time in seconds
  static PPERF_OBJECT_TYPE firstObject( PPERF_DATA_BLOCK pPerfData );
  static PPERF_OBJECT_TYPE nextObject( PPERF_OBJECT_TYPE pObject );
  static PERF_COUNTER_DEFINITION *firstCounter( PERF_OBJECT_TYPE *pObjectDef );
  static PERF_COUNTER_DEFINITION *nextCounter( PERF_COUNTER_DEFINITION *pCounterDef);
  static PERF_INSTANCE_DEFINITION *firstInstance( PERF_OBJECT_TYPE *pObject );
  static PERF_INSTANCE_DEFINITION *nextInstance(PERF_INSTANCE_DEFINITION *pInstance);

  static void grabOffsets ( PPERF_OBJECT_TYPE );
      // pointer to perfdata block - where all the performance data lives.
  static PPERF_DATA_BLOCK pDataBlock;
  static size_t           cbDataBlockAlloc;
  static size_t           cbDataBlockData;
  static int              cAllocs;
  static int              cReallocs;
  static int              cGetSystemPerfDataCalls;
  static int              cPerfDataQueries;
  static double           sPerfDataQueries;
  static struct Offset *offsets;
public:
  static int grabDataBlockStats(int & cA, int & cR, int & cQueries, double & sQueries, size_t & cbAlloc, size_t & cbData);
private:

#endif // WIN32 poop.

  /* Using condor's HashTable template class.  I'm storing a procHashNode, 
     hashed on a pid. */
  static HashTable <pid_t, procHashNode *> *procHash;

  // private data structures:
  static piPTR allProcInfos; // this will be a linked list of 
                             // procinfo structures, one for each process
                             // whose /proc information is available.

#ifndef WIN32
  static std::vector<pid_t> pidList;      // this will be a linked list of all processes
                                  // in the system.  Built by buildpidlist()

  static int pagesize;     // pagesize is the size of memory pages, in k.
                           // It's here so the call getpagesize() only needs
                           // to be called once.

#ifdef LINUX

  static long unsigned boottime; // this is used only in linux.  It
		// represents the number of seconds after the epoch that the
		// machine was booted.  Used in age calculation.
  static long boottime_expiration; // the boottime value will
		// change if the time is adjusted on this machine (by ntpd or afs,
		// for example), so we recompute it when our value expires

#endif // LINUX

#endif // not defined WIN32

  static int MAX_SAMPLES;
  static int DEFAULT_PRECISION_RANGE;

	  // number of birthday time units per second for the given OS
  static double TIME_UNITS_PER_SEC;

}; 


#ifdef WANT_STANDALONE_DEBUG
// The following are for the test programs.

// how many children to you want to create?  5 is a good number for me.
const int NUMKIDS = 5;

// when each child is created, it allocates some memory.  The first
// child allocates "MEMFACTOR" Megs.  The second allocates MEMFACTOR *
// 2 Megs.  etc, etc.  Be careful of thrashing...unless that's what
// you want.
const int MEMFACTOR = 3;

#endif // WANT_STANDALONE_DEBUG

#endif // PROCAPI_H
