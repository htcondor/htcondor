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
#ifndef KILLFAMILY_H
#define KILLFAMILY_H

#include "extArray.h"
#include "condor_uid.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

class ProcAPI; 	// forward reference

class ProcFamily : public Service {
public:
	
	ProcFamily( pid_t pid, priv_state priv, int test_only = 0 );

	~ProcFamily();

	void hardkill();
	void softkill(int sig);
	void suspend();
	void resume();

		// get cpu usage of the family in seconds
	void get_cpu_usage(long & sys_time, long & user_time);

		// get the maxmimum family image size, in kbytes, seen
		// across all snapshots.  note this does _not_ generate
		// a call to takesnapshot() itself.
	void get_max_imagesize(unsigned long & max_image );

	void takesnapshot();

		// Allocates an array for all pids in the current pid family, 
		// sets the given pointer to that array, and returns the
		// number of pids  in the family.  The array must be
		// deallocated with delete.
	int		currentfamily( pid_t* & );	
	int		size() { return family_size; };
	
	void	display();		// dprintf's the existing pid family

	void	setFamilyLogin( char *login );

private:
	enum KILLFAMILY_DIRECTION {
		PATRICIDE, 		// parent die first, then kids
		INFANTICIDE		// children die first, then parent
	}; 

	void spree(int sig, KILLFAMILY_DIRECTION direction);
	void safe_kill(pid_t inpid,int sig);

	int test_only_flag;
	
	pid_t daddy_pid;

	priv_state mypriv;

#ifdef WIN32
	CSysinfo sysinfo;
	ExtArray<HANDLE> familyHandles;
#endif

	void closeFamilyHandles();

	class a_pid {
		public:
			// Constructor - just zero out everything
			a_pid() : pid(0),ppid(0),birthday(0L),
				cpu_user_time(0L),cpu_sys_time(0L) {};
			
			// the pid
			pid_t pid;
			// the parent's pid
			pid_t ppid;
			// the epoch time when process was born
			long birthday;
			// the amount of user cpu time used in seconds
			long cpu_user_time;
			// the amount of system cpu time used in seconds
			long cpu_sys_time;
	};

	ExtArray<a_pid> *old_pids;

	int family_size;

	// total cpu usage for pids which have exited
	long exited_cpu_user_time;
	long exited_cpu_sys_time;

	// total cpu usage for pids which are still alive
	long alive_cpu_user_time;
	long alive_cpu_sys_time;

	unsigned long max_image_size;

	int getPidFamilyByLogin(pid_t *pidFamily);
	char *searchLogin;
};

#endif
