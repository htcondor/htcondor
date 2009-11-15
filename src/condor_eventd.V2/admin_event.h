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

#ifndef __ADMINEVENT_H__
#define __ADMINEVENT_H__

#include "condor_daemon_core.h"
#include "daemon.h"
#include "dc_master.h"
#include "classad_hashtable.h"
#include "MyString.h"

/*

States

*/

const int EVENT_NOW					= 10; // Do this in a asecond
const int EVENT_INIT				= 1;
const int EVENT_HUERISTIC			= 2;
const int EVENT_EVAL_HUERISTIC		= 3;
const int EVENT_SAMPLING			= 4;
const int EVENT_EVAL_SAMPLING		= 5;
const int EVENT_MAIN_WAIT			= 6;
const int EVENT_RESAMPLE			= 7;
const int EVENT_EVAL_RESAMPLE		= 8;
const int EVENT_GO					= 9;
const int EVENT_DONE				= 10;

const int BATCH_SIZE				= 4;

struct StartdStats {
	StartdStats( const char Name[], int Universe, int ImageSize, int LastCheckpoint) :
		universe(Universe), imagesize(ImageSize), lastcheckpoint(LastCheckpoint),
		jobstart(0), slotid(0), remotetime(0), ckptmegs(0), ckptlength(0), 
		ckptmegspersec(0), ckpttime(0), ckptgroup(0), ckptdone(0)
		{ 	
			snprintf( name, 128, "%s", Name ); 
			state[0] = '\0';
			activity[0] = '\0';
			clientmachine[0] = '\0';
			myaddress[0] = '\0';
			jobid[0] = '\0';
			ckptsrvr[0] = '\0';
		}
	char 	name[128];
	char	state[128];
	char	activity[128];
	char 	clientmachine[128];
	char	myaddress[128];
	char	jobid[128];
	char	ckptsrvr[128];
	int 	universe;
	int		imagesize;
	int		lastcheckpoint;
	int 	jobstart;
	int		slotid;
	int 	remotetime;
	// space for managing the benchmarking and checkpointing staging
	int 	ckptmegs;
	int 	ckptlength;
	float   ckptmegspersec;
	int 	ckpttime;
	int 	ckptgroup;
	int 	ckptdone;
};

class AdminEvent : public Service
{
  public:
	// ctor/dtor
	AdminEvent( void );
	~AdminEvent( void );
	int init( void );
	int config( bool init = false );
	int shutdownClean(void);
	int shutdownFast(void);
	int shutdownGraceful(void);

  private:
	// Command handlers
	// Timer handlers
	
	void 		th_DoShutdown( void );
	int 		m_timeridDoShutdown;
	unsigned 	m_intervalDoShutdown;

	void		th_maintainCheckpoints( void );
	int 		m_timerid_maintainCheckpoints;
	unsigned 	m_intervalCheck_maintainCheckpoints;
	unsigned 	m_intervalPeriod_maintainCheckpoints;

	void 		th_DoShutdown_States( void );
	int 		m_timerid_DoShutdown_States;
	unsigned 	m_intervalCheck_DoShutdown_States;
	unsigned 	m_intervalPeriod_DoShutdown_States;

	time_t		m_shutdownStart;
	time_t 		m_shutdownEnd;
	unsigned	m_shutdownMegs; /* number of megs to checkpoint before shutdown */

	ClassAdList m_CkptBenchMarks;
	ClassAdList m_PollingStartdAds;

	// Event Handling Methods

		/// Determine the current attributes for a shutdown
	int check_Shutdown( bool init = false );
	int do_checkpoint_shutdown( bool init = false );
	int process_ShutdownTime( char *req_time );
	int FetchAds_ByConstraint( const char *constraint );
	int changeState( int howsoon = EVENT_NOW, int newstate = EVENT_INIT );
	int loadCheckPointHash( int megs );
	int getShutdownDelta( void );

	// Action Methods

	int pollStartdAds( ClassAdList &adsList, char *sinful, char *name );
		/// Send a checkpoint to the job
	int sendCheckpoint( char *sinful, char *name );
		/// Send a vacate to the job
	int sendVacateClaim( char *sinful, char *name );
		/**
		Routine takes a hash of jobs which met the standard universe
		requirement and some constraint and if we have met a threshold
		limit for total image size, will create a smaller hash with jobs
		which are requested to vacate.
		*/

	// Benchmarking methods

		/** 
		Place a benchmark statistic(classad) into a list of benchmarks 
		into the m_CkptBenchMarks hash.
		*/
	int benchmark_insert( float megspersec, int megs, int time, char *name);
		/*
		This routine first shows the current benchmarks being collected
		on a particular image size amount from hash m_CkptBenchMarks and 
		then shows the history of performance from the prior batches of 
		benchmarks in the hash m_CkptBatches.
		*/
	int benchmark_show_results( );
	// Print Methods
	int standardUDisplay();
	int standardUDisplay_StartdStats();

	// Processing events
	int standardUProcess( int batchsz = 0, bool vacate = false );
	int totalRunningJobs();
	int empty_Hashes();
	int SS_store(StartdStats *ss, int duration);
	int spoolClassAd( ClassAd * ca_shutdownRate, char *direction );

	// Operation Markers
	int 		m_mystate;

	/* Administrator Input div 8 for bits to bytes*/
	float 		m_newshutdownAdminRate;

	time_t 		m_shutdownTime;			/* established shutdown time */
	time_t 		m_newshutdownTime;		/* new shutdown time being considered */
	time_t 		m_shutdownDelta;		/* time till shutdown event occurs */
	time_t 		m_timeNow;				/* time till shutdown event occurs */
	time_t 		m_timeSinceNow;			/* time till shutdown event occurs */

	MyString 	m_shutdownTarget;		/* what machine(s) */
	MyString 	m_newshutdownTarget;	/* what new machine(s) */
	MyString 	m_shutdownConstraint;	/* which machines? */

	unsigned 	m_shutdownSize;			
	unsigned 	m_newshutdownSize;	

	// Hash for processing scheduled checkpoint
	HashTable<HashKey, StartdStats *> m_JobNodes_su;

	// storage
	ClassAdList m_collector_query_ads;
	ClassAdList m_claimed_standard;
	ClassAdList m_fromStartd;
	FILE		*m_spoolStorage;
	ClassAd		*m_lastShutdown;

};




#endif//__ADMINEVENT_H__
