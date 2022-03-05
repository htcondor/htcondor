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

/* Here are the necessary #includes */
#include "condor_common.h"
#include "condor_system.h"
#include "condor_pidenvid.h"
#include "HashTable.h"
#include "extArray.h"

#include "processid.h"

#include <dirent.h>        // get /proc entries (directory stuff)
#include <sys/types.h>     // various types needed.
#include <time.h>          // use of time() for process age.

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
typedef unsigned long long birthday_t;
#define PROCAPI_BIRTHDAY_FORMAT "%llu"

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

  // the owner of this process
  uid_t owner;

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
typedef struct procInfoRaw {

		// Virtual size and working set size
		// are reported as 64-bit quantities
		// on Windows
	int64_t imgsize;
	int64_t rssize;

	unsigned long pssize;
	bool pssize_available;

	long minfault;
	long majfault;
	pid_t pid;
	pid_t ppid;
	uid_t owner;

	unsigned long proc_flags;
} procInfoRaw;

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

  static piPTR procFamily;
  static int buildFamily( pid_t,
                          PidEnvID *,
                          int & );
  static void deallocProcFamily();
  static int getNumProcs();

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

  /**
    * Calculate the Proportional Set Size, a Linux-specific metric
    * for memory usage taking into account memory sharing.
    *
    * This will operate directly on the procInfo reference; if this
    * function isn't called, PSS information will not be available through
    * the "normal" means.
    */
  static int getPSSInfo( pid_t pid, procInfo& procInfo, int &status );

  /**
    * Helper function for measuring code performance. get the cpu usage and
    * memory usage of the given process.
    */
  static size_t getBasicUsage(pid_t pid, double * puser_time, double * psys_time=NULL);

  static int getProcInfoListStats(double & sOverall,
                                  int & cReg, double & sReg,
                                  int & cPid, double & sPid,
                                  int & cCPU, double & sCPU);
  static int grabDataBlockStats(int & cA, int & cR, int & cQueries, double & sQueries, size_t & cbAlloc, size_t & cbData);

 private:

  /** Default constructor.  It's private so that no one really
	  instantiates a ProcAPI object anywhere. */
  ProcAPI() {};

};

#endif // PROCAPI_H
