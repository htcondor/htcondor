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
	int		kill(int);
	int		killpg(int);
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
	int		sigkillStarter( void );

	void	publish( ClassAd* ad, amask_t mask, StringList* list );

	bool	satisfies( ClassAd* job_ad, ClassAd* mach_ad );
	bool	provides( const char* ability );

	void	setAd( ClassAd* ad );
	void	setPath( const char* path );
	void	setIsDC( bool is_dc );

	void	setClaim( Claim* c );
	void	setPorts( int, int );
	void	setCODArgs( const char* keyword );
	char*	getCODKeyword( void ) { return s_cod_keyword; };

	void	printInfo( int debug_level );

private:

		// methods
	int		reallykill(int, int);
	int		execOldStarter( void );
	int		execCODStarter( void );
	int		execDCStarter( Stream* s );
	int		execDCStarter( const char* args, Stream* s );
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
	char*	s_cod_keyword;
};

#endif /* _CONDOR_STARTD_STARTER_H */
