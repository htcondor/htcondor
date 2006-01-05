/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
/*
  This file defines the Starter class, used by the startd to keep
  track of a resource's starter process.  

  Written 10/6/97 by Derek Wright <wright@cs.wisc.edu>
*/

#ifndef _CONDOR_STARTD_STARTER_H
#define _CONDOR_STARTD_STARTER_H

#include "../condor_procapi/procapi.h"
#include "killfamily.h"
class Claim;

class Starter : public Service
{
public:
	Starter();
	Starter( const Starter& s );
	~Starter();


	void	dprintf( int, char* ... );

	char*	path() {return s_path;};
	time_t	birthdate( void ) {return s_birthdate;};
	time_t	last_snapshot( void ) {return s_last_snapshot;};
	bool	kill(int);
	bool	killpg(int);
	void	killkids(int);
	void	exited();
	pid_t	pid() {return s_pid;};
	bool	is_dc() {return s_is_dc;};
	bool	isCOD(); 
	bool	active();
	void	set_last_snapshot( time_t val ) { s_last_snapshot = val; };
	float	percentCpuUsage( void );
	unsigned long	imageSize( void );

	int		spawn( time_t now, Stream* s );

	bool	killHard( void );
	bool	killSoft( void );
	bool	suspend( void );
	bool	resume( void );
	
		// Send SIGKILL to starter + process group (called by our kill
		// timer if we've been hardkilling too long).
	bool	sigkillStarter( void );

	void	publish( ClassAd* ad, amask_t mask, StringList* list );

	bool	satisfies( ClassAd* job_ad, ClassAd* mach_ad );
	bool	provides( const char* ability );

	void	setAd( ClassAd* ad );
	void	setPath( const char* path );
	void	setIsDC( bool is_dc );

	void	setClaim( Claim* c );
	void	setPorts( int, int );

	void	printInfo( int debug_level );

	char*	getIpAddr( void );

private:

		// methods
	bool	reallykill(int, int);
	int		execOldStarter( void );
	int		execCODStarter( void );
	int		execDCStarter( Stream* s );
	int		execDCStarter( ArgList const &args, Env const *env, 
						   int std_fds[], Stream* s );
	void	initRunData( void );

	int		startKillTimer( void );	    // Timer for how long we're willing 
	void	cancelKillTimer( void );	// to "hardkill" before we SIGKILL

		// helpers related to computing percent cpu usage
	void	printPidFamily( int dprintf_level, char* header );
	void	recomputePidFamily( time_t now = 0 );

		// data that will be the same across all instances of this
		// starter (i.e. things that are valid for copying)
	ClassAd* s_ad;
	char*	s_path;
	bool 	s_is_dc;

		// data that only makes sense once this Starter object has
		// been assigned to a given resource and spawned.
	Claim*	s_claim;
	pid_t	s_pid;
	pid_t*	s_pidfamily;
	int		s_family_size;
	ProcFamily*	s_procfam;
	time_t	s_birthdate;
	time_t	s_last_snapshot;
	int		s_kill_tid;		// DC timer id for hard killing
	procInfo	s_pinfo;	// aggregate ProcAPI info for starter & job
	int		s_port1;
	int		s_port2;
};

#endif /* _CONDOR_STARTD_STARTER_H */
