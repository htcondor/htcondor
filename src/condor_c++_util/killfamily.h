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

class ProcAPI; 	// forward reference

class ProcFamily {
public:
	
	ProcFamily( pid_t pid, priv_state priv, int test_only = 0 );
	ProcFamily( pid_t pid, priv_state priv, ProcAPI* papi );

	~ProcFamily();

	void hardkill();
	void softkill(int sig);
	void suspend();
	void resume();

	void takesnapshot();

		// Allocates an array for all pids in the current pid family, 
		// sets the given pointer to that array, and returns the
		// number of pids  in the family.  The array must be
		// deallocated with delete.
	int		currentfamily( pid_t* & );	
	int		size() { return family_size; };
	
	void	display();		// dprintf's the existing pid family

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

	ProcAPI *procAPI;

	class a_pid {
		public:
			// Constructor - just zero out everything
			a_pid() : pid(0),ppid(0),birthday(0L) {};
			
			// the pid
			pid_t pid;
			// the parent's pid
			pid_t ppid;
			// the epoch time when process was born
			long birthday;
	};

	ExtArray<a_pid> *old_pids;

	int family_size;
	int needs_free;
};

#endif
