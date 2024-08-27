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

/*
  This file defines the Starter class, used by the startd to keep
  track of a resource's starter process.  

  Written 10/6/97 by Derek Wright <wright@cs.wisc.edu>
*/

#ifndef _CONDOR_STARTD_STARTER_H
#define _CONDOR_STARTD_STARTER_H

#include "../condor_procapi/procapi.h"
#include "../condor_procd/proc_family_io.h"

class Claim;

class Starter : public Service
{
public:
	Starter();
	~Starter();

	static void config();

	void	dprintf( int, const char* ... );
	void	set_dprintf_prefix(const char * prefix) { if (prefix) { s_dpf = prefix; } else { s_dpf.clear(); } }

	char*	path() {return s_path;};
	time_t	birthdate( void ) const {return s_birthdate;};
	time_t	got_update(void) const {return s_last_update_time;}
	bool	got_final_update(void) const {return s_got_final_update;}
	bool	signal(int);
	bool	killfamily();
	void	exited(Claim *, int status);
	pid_t 	spawn(Claim *, time_t now, Stream* s );
	pid_t	pid() const {return s_pid;};
	bool	active() const;
	const ProcFamilyUsage & updateUsage(void);


	void	setReaperID( int reaper_id ) { s_reaper_id = reaper_id; };
	bool    notYetReaped() const { return (s_pid != 0) && ! s_was_reaped; }
	void    setOrphanedJob(ClassAd * job);

	void    setExecuteDir( char const * dir ) { s_execute_dir = dir; }
		// returns NULL if no execute directory set, o.w. returns the value
		// of EXECUTE that is passed to the starter
		// If encryption is enabled, this may be different than the value
		// originally set.
	const char *executeDir() { return s_execute_dir.empty() ? nullptr : s_execute_dir.c_str(); }
	const char *logicalVolumeName() { return s_lv_name.empty() ? nullptr : s_lv_name.c_str(); }

	bool	killHard( int timeout );
	bool	killSoft( int timeout, bool state_change = false );
	bool	suspend( void );
	bool	resume( void );

	bool	holdJob(char const *hold_reason,int hold_code,int hold_subcode,bool soft,int timeout);

		// Send SIGKILL to starter + process group (called by our kill
		// timer if we've been hardkilling too long).
	void	sigkillStarter( int timerID = -1 );

		// Escalate to a fast shutdown of the job.
		// Called by our softkill timer
	void softkillTimeout( int timerID = -1 );
	
	static void	publish( ClassAd* ad );

#if HAVE_BOINC
	bool	isBOINC( void ) const { return s_is_boinc; };
	void	setIsBOINC( bool is_boinc ) { s_is_boinc = is_boinc; };
#endif /* HAVE_BOINC */

	void	printInfo( int debug_level );

	char const*	getIpAddr( void );

	int receiveJobClassAdUpdate( Stream *stream );

	void holdJobCallback(DCMsgCallback *cb);

private:

		// methods
	bool	reallykill(int, bool);
	int		execJobPipeStarter( Claim * );
	int		execDCStarter( Claim *, Stream* s );
		// claim is optional here, and may be NULL (e.g. boinc) but it may NOT be null when glexec is enabled. (sigh)
	int		execDCStarter( Claim *, ArgList const &args, Env const *env,
						   int std_fds[], Stream* s );
#if HAVE_BOINC
	int 	execBOINCStarter( Claim * );
#endif /* HAVE_BOINC */

	void	initRunData( void );

	int	startKillTimer( int timeout );		// Timer for how long we're willing
	void	cancelKillTimer( void );	// to "hardkill" before we SIGKILL
	int startSoftkillTimeout( int timeout );
		// choose EXECUTE directory for starter
	void    finalizeExecuteDir( Claim * );

		// data that will be the same across all instances of this
		// starter (i.e. things that are valid for copying)
	static ClassAd* s_ad; // starter capabilities ad, (not the job ad!)
	static char* s_path;

		// data that only makes sense once this Starter object has
		// been assigned to a given resource and spawned.
	std::string     s_dpf; // prefix for all dprintf messages (normally the slot id)
	pid_t           s_pid;
	ProcFamilyUsage s_usage;
	time_t          s_birthdate;
	time_t          s_last_update_time;
	double          s_vm_cpu_usage;
	int             s_num_vm_cpus; // number of CPUs allocated to the hypervisor, used with additional_cpu_usage correction
	int             s_kill_tid;		// DC timer id for hard killing
	int             s_softkill_tid;
	int             s_hold_timeout;
	bool            s_is_vm_universe;
#if HAVE_BOINC
	bool            s_is_boinc;
#endif /* HAVE_BOINC */
	bool            s_was_reaped;
	bool            s_created_execute_dir; // should we cleanup s_execute_dir
	bool            s_got_final_update;
	bool            s_lv_encrypted{false};
	int             s_reaper_id;
	int             s_exit_status;
	ClassAd *       s_orphaned_jobad;  // the job ad is transferred from the Claim to here if the claim is deleted before the starter is reaped.
	ReliSock*       s_job_update_sock;
	std::string     s_execute_dir;
	std::string     s_lv_name; // LogicalVolume name for use with LVM 
	DCMsgCallback*  m_hold_job_cb;
	std::string     m_starter_addr;
};

// living (or unreaped) starters live in a global data structure and can be looked up by PID.
Starter *findStarterByPid(pid_t pid);

#endif /* _CONDOR_STARTD_STARTER_H */
