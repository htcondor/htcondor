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

#ifndef KILLFAMILY_H
#define KILLFAMILY_H

#include "extArray.h"
#include "condor_uid.h"
#include "condor_daemon_core.h"
#include "../condor_procapi/procapi.h"
#include "condor_pidenvid.h"

class ProcAPI; 	// forward reference

class KillFamily : public Service {
public:
	
	KillFamily( pid_t pid, priv_state priv, int test_only = 0 );

	~KillFamily();

	void hardkill();
	void softkill(int sig);
	void suspend();
	void resume();

		// get cpu usage of the family in seconds
	void get_cpu_usage(long & sys_time, long & user_time);

		// get the maxmimum family image size, in kbytes, seen
		// across all snapshots.  note this does _not_ generate
		// a call to takesnapshot() itself.
	void get_max_imagesize(unsigned long & max_image ) const;

	void takesnapshot();

		// Allocates an array for all pids in the current pid family, 
		// sets the given pointer to that array, and returns the
		// number of pids  in the family.  The array must be
		// deallocated with delete.
	int		currentfamily( pid_t* & );	
	int		size() const { return family_size; };
	
	void	display();		// dprintf's the existing pid family

	void	setFamilyEnvironmentID( PidEnvID* penvid );
	void	setFamilyLogin( const char *login );

private:
	enum KILLFAMILY_DIRECTION {
		PATRICIDE, 		// parent die first, then kids
		INFANTICIDE		// children die first, then parent
	}; 

	class a_pid;

	void spree(int sig, KILLFAMILY_DIRECTION direction);
	void safe_kill(a_pid *pid, int sig);

	int test_only_flag;
	
	pid_t daddy_pid;

	priv_state mypriv;

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
			birthday_t birthday;
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

	PidEnvID m_penvid;

	char *searchLogin;
};

#endif
