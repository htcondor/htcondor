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
	Starter( const Starter& s );
	~Starter();


	void	dprintf( int, const char* ... );

	char*	path() {return s_path;};
	time_t	birthdate( void ) {return s_birthdate;};
	bool	kill(int);
	bool	killpg(int);
	void	killkids(int);
	void	exited(int status);
	pid_t	pid() {return s_pid;};
	bool	is_dc() {return s_is_dc;};
	ClaimType	claimType(); 
	bool	active();
	float	percentCpuUsage( void );
	unsigned long	imageSize( void );

	int		spawn( time_t now, Stream* s );
	void	setReaperID( int reaper_id ) { s_reaper_id = reaper_id; };

	void    setExecuteDir( char const * dir ) { s_execute_dir = dir; }
		// returns NULL if no execute directory set, o.w. returns the value
		// of EXECUTE that is passed to the starter
	char const *executeDir();

	bool	killHard( void );
	bool	killSoft( void );
	bool	suspend( void );
	bool	resume( void );

	bool    holdJob(char const *hold_reason,int hold_code,int hold_subcode);

		// Send SIGKILL to starter + process group (called by our kill
		// timer if we've been hardkilling too long).
	bool	sigkillStarter( void );

	void	publish( ClassAd* ad, amask_t mask, StringList* list );

	bool	satisfies( ClassAd* job_ad, ClassAd* mach_ad );
	bool	provides( const char* ability );

	void	setAd( ClassAd* ad );
	void	setPath( const char* path );
	void	setIsDC( bool is_dc );

#if HAVE_BOINC
	bool	isBOINC( void ) { return s_is_boinc; };
	void	setIsBOINC( bool is_boinc ) { s_is_boinc = is_boinc; };
#endif /* HAVE_BOINC */

	void	setClaim( Claim* c );
	void	setPorts( int, int );

	void	printInfo( int debug_level );

	char const*	getIpAddr( void );

	int receiveJobClassAdUpdate( Stream *stream );

	void holdJobCallback(DCMsgCallback *cb);

private:

		// methods
	bool	reallykill(int, int);
	int		execOldStarter( void );
	int		execJobPipeStarter( void );
	int		execDCStarter( Stream* s );
	int		execDCStarter( ArgList const &args, Env const *env, 
						   int std_fds[], Stream* s );
#if HAVE_BOINC
	int 	execBOINCStarter( void );
#endif /* HAVE_BOINC */

#if !defined(WIN32)
		// support for spawning starter using glexec
	bool    prepareForGlexec( const ArgList&,
	                          const Env*,
	                          const int[3],
	                          ArgList&,
	                          Env&,
	                          int[3],
	                          int[2],
	                          int&);
	bool    handleGlexecEnvironment(pid_t, Env&, int[2], int);
	void    cleanupAfterGlexec();
#endif

	void	initRunData( void );

	int		startKillTimer( void );	    // Timer for how long we're willing 
	void	cancelKillTimer( void );	// to "hardkill" before we SIGKILL

		// choose EXECUTE directory for starter
	void    finalizeExecuteDir( void );

		// data that will be the same across all instances of this
		// starter (i.e. things that are valid for copying)
	ClassAd* s_ad;
	char*	s_path;
	bool 	s_is_dc;

		// data that only makes sense once this Starter object has
		// been assigned to a given resource and spawned.
	Claim*          s_claim;
	pid_t           s_pid;
	ProcFamilyUsage s_usage;
	time_t          s_birthdate;
	int             s_kill_tid;		// DC timer id for hard killing
	int             s_port1;
	int             s_port2;
#if HAVE_BOINC
	bool            s_is_boinc;
#endif /* HAVE_BOINC */
	int             s_reaper_id;
	ReliSock*       s_job_update_sock;
	MyString        s_execute_dir;
	DCMsgCallback*  m_hold_job_cb;
	MyString        m_starter_addr;
};

#endif /* _CONDOR_STARTD_STARTER_H */
