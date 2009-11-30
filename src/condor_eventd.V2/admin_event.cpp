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

#include "condor_common.h"
#include "condor_api.h"
#include "condor_daemon_core.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "daemon.h"
#include "daemon_types.h"
#include "dc_collector.h"
#include "dc_master.h"
#include "dc_startd.h"
#include "admin_event.h"
#include "classad_hashtable.h"
#include "directory.h"

int numStartdStats = 0;
int ClaimRunTimeSort(ClassAd *job1, ClassAd *job2, void *data);

AdminEvent::AdminEvent( void ) : 
	m_JobNodes_su(1024,hashFunction)
{
	m_mystate = EVENT_INIT;
	m_timeridDoShutdown = -1;
	m_intervalDoShutdown = 60;

	m_timerid_DoShutdown_States = 0;
	m_intervalCheck_DoShutdown_States = 5;
	m_intervalPeriod_DoShutdown_States = 0;

	m_timerid_maintainCheckpoints = -1;
	m_intervalCheck_maintainCheckpoints = 5;
	m_intervalPeriod_maintainCheckpoints = 10;

	m_shutdownTime = 0;
	m_newshutdownTime = 0;
	m_shutdownDelta = 0;

	m_shutdownTarget = "";
	m_shutdownConstraint = "";

	m_shutdownMegs = 0;
	m_shutdownStart = 0;
	m_shutdownEnd = 0;
	m_shutdownSize = 0;
	m_newshutdownSize = 0;
}

AdminEvent::~AdminEvent( void )
{
	delete m_lastShutdown;
}

int
AdminEvent::init( void )
{
	// Read our Configuration parameters
	config( true );
	return 0;
}

int
AdminEvent::shutdownFast( void )
{
	dprintf(D_ALWAYS,"shutdownFast\n");

	shutdownClean();
	return 0;
}

int
AdminEvent::shutdownGraceful( void )
{
	dprintf(D_ALWAYS,"shutdownGraceful\n");

	shutdownClean();
	return 0;
}

int
AdminEvent::shutdownClean( void )
{
	ClassAd *ad;
	char *tmp;

	tmp = param("SPOOL");
	if(tmp) {
		if(spoolClassAd(m_lastShutdown,"out") == 1) {
			dprintf(D_ALWAYS,"Failed to get/create initial spool classad\n");
		}
		free(tmp);
	}

	dprintf(D_ALWAYS,"shutdownClean .. cleaning classad lists\n");

	m_PollingStartdAds.Rewind();
	while( (ad = m_PollingStartdAds.Next()) ){
		m_PollingStartdAds.Delete(ad);
	}

	m_CkptBenchMarks.Rewind();
	while( (ad = m_CkptBenchMarks.Next()) ){
		m_CkptBenchMarks.Delete(ad);
	}

	m_claimed_standard.Rewind();
	while( (ad = m_claimed_standard.Next()) ){
		m_claimed_standard.Delete(ad);
	}

	dprintf(D_ALWAYS,"shutdownClean .. cleaning hashes \n");

	empty_Hashes();
	fclose(m_spoolStorage);

	return 0;
}

int
AdminEvent::config( bool initial )
{
	// Initial values
	if ( initial ) {
		dprintf(D_FULLDEBUG, "config(true) \n");
		m_timeridDoShutdown = -1;
		m_intervalDoShutdown = 60;

		m_shutdownTime = 0;
		m_shutdownTarget = "";
		m_mystate = EVENT_SAMPLING;
	} else {
		dprintf(D_FULLDEBUG, "config(false) \n");
	}

	empty_Hashes();

	/* Lets check events which might be set */
	check_Shutdown(initial);
	return 0;
}

int
AdminEvent::getShutdownDelta( void )
{
	int now = time(NULL);
	m_shutdownDelta = m_shutdownTime - now;
	dprintf(D_FULLDEBUG,
		"getShutdownDelta: Now<%d> m_shutdownTime<%d> New Delta<%d>\n",
		now, (int)m_shutdownTime, (int)m_shutdownDelta);
	return(m_shutdownDelta);
}

int
AdminEvent::check_Shutdown( bool initial )
{
	char *timeforit;
	char *tmp;
	m_timeNow = time(NULL);

	dprintf(D_FULLDEBUG, "Checking For Shutdown\n");

	// Get EVENTD_SHUTDOWN_TIME parameter
	timeforit = param( "EVENTD_SHUTDOWN_TIME" );
	if( timeforit ) {
		 dprintf(D_FULLDEBUG, "EVENTD_SHUTDOWN_TIME is %s\n",
		 		timeforit);
		 process_ShutdownTime( timeforit );
		 free( timeforit );
	}

	/* Are we currently set with a shutdown? */
	if(m_timeridDoShutdown >= 0)
	{
		/* 
		  we have one set and if it is not the same time, calculate when
		  and reset the timer.
		*/
		if( m_shutdownTime == m_newshutdownTime){
			dprintf(D_ALWAYS, 
				"ignore repeat shutdown event. Time<<%d>> New Time<<%d>>\n",
				(int)m_shutdownTime, (int)m_newshutdownTime);
		} else {
			if(m_newshutdownTime != 0){
				if(m_newshutdownTime > m_timeNow) {
					m_intervalDoShutdown = 
							(unsigned)(m_newshutdownTime - m_timeNow);
					m_shutdownTime = m_newshutdownTime;
					if (( m_timeridDoShutdown < 0 ) 
							&& (m_intervalDoShutdown > 60)) 
					{
						dprintf(D_ALWAYS, 
							"shutdown event(reset timer) <<%d>> from now\n",
							m_intervalDoShutdown);
						m_timeridDoShutdown = daemonCore->Reset_Timer(
							m_timeridDoShutdown, m_intervalDoShutdown, 0);
					} else {
						dprintf(D_ALWAYS, 
							"chk_Shtdwn: Shutdown denied (why)\n");
					}
				} else {
					dprintf(D_ALWAYS, 
						"chk_Shtdwn: Shutdown denied (past or too close\n");
				}
			}
		}
	} else {
		/* this is a new timer request */
		if(m_newshutdownTime != 0){
			if(m_newshutdownTime > m_timeNow) {
				m_intervalDoShutdown = 
					(unsigned)(m_newshutdownTime - m_timeNow);
				m_shutdownTime = m_newshutdownTime;
				if ((m_timeridDoShutdown < 0) && (m_intervalDoShutdown > 60)) {
					dprintf(D_FULLDEBUG, 
						"shutdown event(register timer) <<%d>> from now\n",
						m_intervalDoShutdown);
					m_timeridDoShutdown = daemonCore->Register_Timer(
						m_intervalDoShutdown,
						(TimerHandlercpp)&AdminEvent::th_DoShutdown,
						"AdminEvent::DoShutdown()", this );
				} else {
						dprintf(D_ALWAYS, 
							"chk_Shtdwn: Shutdown denied (why)\n");
				}
			} else {
				dprintf(D_ALWAYS, 
					"chk_Shtdwn: denied either past or too close!\n");
			}
		}
	}

	tmp = param("SPOOL");
	if(tmp) {
		if(spoolClassAd(m_lastShutdown,"in") == 1) {
			dprintf(D_ALWAYS,"Failed to get/create initial spool classad\n");
		}
		m_lastShutdown->dPrint(D_FULLDEBUG);
		free(tmp);
	} else {
		EXCEPT( "SPOOL not defined!" );
	}

	// Get EVENTD_ADMIN_MEGABITS_SEC parameter
	tmp = param( "EVENTD_ADMIN_MEGABITS_SEC" );
	if(tmp) {
		float megspersec = 0.0;
		/* adjust to megabytes */
		m_newshutdownAdminRate = (atof(tmp)/8.0);
		dprintf(D_ALWAYS, 
				"EVENTD_ADMIN_MEGABITS_SEC is %d(Megs %f)\n",
				atoi(tmp), m_newshutdownAdminRate);
	
		/* do we have usable history */
		m_lastShutdown->LookupFloat( "LastShutdownRate", megspersec );
		if(megspersec > 0.0) {
			/* moderate to a compromise */
			m_newshutdownAdminRate = 
				((m_newshutdownAdminRate + megspersec)/2.0);
			dprintf(D_ALWAYS, 
				"Historic LastShutdownRate is %f modified now to is %f\n",
				megspersec, m_newshutdownAdminRate);
		}
		free( tmp );
	} else {
		EXCEPT( "EVENTD_ADMIN_MEGABITS_SEC not defined!" );
	}

	// Get EVENTD_SHUTDOWN_CONSTRAINT parameter
	tmp = param( "EVENTD_SHUTDOWN_CONSTRAINT" );
	if( tmp ) {
		m_shutdownConstraint = tmp;
		dprintf(D_ALWAYS, 
				"EVENTD_SHUTDOWN_CONSTRAINT is %s\n",tmp);
		free( tmp );
	}

	if(initial) {
		changeState(3, m_mystate);
	}
	return(0);
}

int
AdminEvent::changeState( int howsoon, int newstate )
{
	dprintf(D_ALWAYS, "**************************************************\n");
	dprintf(D_ALWAYS, "Changing to state <<<<< %d >>>>> in %d\n"
			,newstate,howsoon);
	dprintf(D_ALWAYS, "**************************************************\n");
	if(m_timerid_DoShutdown_States > 0) {
		daemonCore->Reset_Timer(m_timerid_DoShutdown_States, 
			howsoon);
	} else {
		m_timerid_DoShutdown_States = daemonCore->Register_Timer(
			howsoon,
			//m_intervalCheck_DoShutdown_States,
			(TimerHandlercpp)&AdminEvent::th_DoShutdown_States,
			"AdminEvent::DoShutdown_States()", this );
	}
	return(0);
}

int
AdminEvent::spoolClassAd( ClassAd * ca_shutdownRate, char *direction )
{
	char *tmp;
	char *tmp2;
	MyString spoolHistory;

	tmp = param("SPOOL");
	if(!tmp) {
		dprintf(D_ALWAYS,"SPOOL not defined!!!!!!!!!!\n");
		return(1);
	}

	tmp2 = dircat(tmp,"EventdShutdownRate.log");
	spoolHistory = tmp2;

	free( tmp );
	delete[] tmp2;

	if(strcmp(direction,"in") == MATCH) {

	// In

		m_spoolStorage = safe_fopen_wrapper((const char *)spoolHistory.Value(),"r");
		if(m_spoolStorage != NULL) {
			int isEOF=0, error=0, empty=0;
			m_lastShutdown = 
				new ClassAd(m_spoolStorage,"//*",isEOF,error,empty);
			if(m_lastShutdown != NULL) {
				dprintf(D_ALWAYS,"Got initial classad from spool(%s)\n",
					spoolHistory.Value());
				fclose(m_spoolStorage);
				m_lastShutdown->dPrint(D_FULLDEBUG);
			} else {
				dprintf(D_ALWAYS,"Failed to get initial spool classad(%s)\n",
					spoolHistory.Value());
				return(1);
			}
		} else {
			char 	line[100];
			float 	lastrate = 0.0;

			dprintf(D_ALWAYS,"Failed to open from spool(%s/%d)\n",
				spoolHistory.Value(),errno);
			m_lastShutdown = new ClassAd();
			sprintf(line, "%s = %f", "LastShutdownRate", lastrate);
			m_lastShutdown->Insert(line);
			m_lastShutdown->dPrint(D_FULLDEBUG);
			m_spoolStorage = safe_fopen_wrapper((const char *)spoolHistory.Value(),"w+");
			if(m_spoolStorage != NULL) {
				m_lastShutdown->fPrint(m_spoolStorage);
				fclose(m_spoolStorage);
			} else {
				dprintf(D_ALWAYS,"Failed to open(w) from spool(%s/%d)\n",
					spoolHistory.Value(),errno);
				return(1);
			}
		}
		return(0);
	} else if(strcmp(direction,"out") == MATCH) {

	// Out

		m_spoolStorage = safe_fopen_wrapper((const char *)spoolHistory.Value(),"w");
		if(m_spoolStorage != NULL) {
			m_lastShutdown->fPrint(m_spoolStorage);
			fclose(m_spoolStorage);
		} else {
			dprintf(D_ALWAYS,"Failed to open(w) from spool(%s/%d)\n",
				spoolHistory.Value(),errno);
			return(1);
		}
		return(0);
	} else {
		return(1);
	}
}

/*

		States

*/

void 
AdminEvent::th_DoShutdown_States( void )
{
	int secondsforvacates = 0;
	int shutdowndelta = 0;
	ClassAd *ad;

	/**

	Handle Shutdown States.....

	Until happy call
		do_checkpoint_samples
	Then set up a callback allowing time to collect current ads
		set up the number of runs through to make
		start doing batches of vacates
	So, how long has it been since we benchmarked the current
		network for vacate optimal throughput????

	Consider recounting how much data is in images of CURRENT
		standard universe jobs.... Set times and alarms based
		on this new polling... It could have been a week ago
		that they set this up even though our currently understood
		time in this code is today......

	**/

	dprintf(D_ALWAYS, "$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\n");
	dprintf(D_ALWAYS, "th_DoShutdown_States(STATE = %d) \n",m_mystate);
	dprintf(D_ALWAYS, "$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\n");

	switch(m_mystate)
	{
		case EVENT_INIT:
			dprintf(D_ALWAYS, 
				"th_DoShutdown_States( no work here - EVENT_INIT: )\n");
			dprintf(D_ALWAYS, 
				"th_DoShutdown_States( Call changeState(m_mystate)- <<%d>>: )\n"
				,m_mystate);
			changeState(EVENT_NOW, m_mystate);
			break;

		case EVENT_SAMPLING:
		{
			dprintf(D_ALWAYS, 
				"th_DoShutdown_States( work here - EVENT_SAMPLING: )\n");
			// how much is running? estimate when to wake up by total image size
			// and the admin checkpointing capacity
			check_Shutdown(false);
			
			char* m_sdc_value = strdup( m_shutdownConstraint.Value() );
			FetchAds_ByConstraint( m_sdc_value );
			free( m_sdc_value );
			
			m_shutdownMegs = totalRunningJobs();
			m_mystate = EVENT_EVAL_SAMPLING;
			changeState(EVENT_NOW, m_mystate);
			break;
		}
		case EVENT_EVAL_SAMPLING:
			dprintf(D_ALWAYS, 
				"( Calculate preliminary wakeup - EVENT_EVAL_SAMPLING: )\n");
			secondsforvacates = (int)(m_shutdownMegs/m_newshutdownAdminRate);
			dprintf(D_ALWAYS,
				"(m_shutdownMegs/m_newshutdownAdminRate) = %d\n"
				,secondsforvacates);
			/** 
				we always want to add 10 minutes of time for looking
				again in case the load has changed dramatically
			**/
			secondsforvacates = ((secondsforvacates * 2) + 600);
			dprintf(D_ALWAYS,
				" resample  = %d before shutdown\n",secondsforvacates);
			shutdowndelta = getShutdownDelta();

			/** 
				We are trying to resample prior to timer action and so optimally
				we double the time we have estimated for shutdown checkpoints
				and set a timer for a wakeup. If we have less then that amount 
				left we'll move to a final wakeup for the checkpoints.
			**/

			if(shutdowndelta >= secondsforvacates) {
				int locdelta = (shutdowndelta - secondsforvacates);
				dprintf(D_ALWAYS, " resample  in %d seconds\n",locdelta);
				m_mystate = EVENT_RESAMPLE;

				m_claimed_standard.Rewind();
				while( (ad = m_claimed_standard.Next()) ){
					m_claimed_standard.Delete(ad);
				}

				changeState(locdelta, m_mystate);
				//changeState(10, m_mystate);
			} else {
				dprintf(D_ALWAYS, " EVENT_GO now!\n");
				m_mystate = EVENT_GO;
				changeState(EVENT_NOW, m_mystate);
			}

			break;

		case EVENT_RESAMPLE:
			dprintf(D_ALWAYS, 
				"th_DoShutdown_States( work here - EVENT_RESAMPLE: )\n");
			// how much is running? estimate when to wake up by total image size
			// and the admin checkpointing capacity. Final check before GO!
			check_Shutdown(false);
			FetchAds_ByConstraint(m_shutdownConstraint.Value());
			m_shutdownMegs = totalRunningJobs();
			m_mystate = EVENT_EVAL_RESAMPLE;
			changeState(EVENT_NOW, m_mystate);
			break;

		case EVENT_EVAL_RESAMPLE:
			dprintf(D_ALWAYS, 
				"( Calculate final wakeup - EVENT_EVAL_RESAMPLE: )\n");
			secondsforvacates = (int)(m_shutdownMegs/m_newshutdownAdminRate);
			secondsforvacates += 600; /* add 10 minutes */
			dprintf(D_ALWAYS,
				"(m_shutdownMegs/m_newshutdownAdminRate) = %d\n"
				,secondsforvacates);
			dprintf(D_ALWAYS,
				" resample  = %d before shutdown\n",secondsforvacates);
			shutdowndelta = getShutdownDelta();

			/** 
			**/

			if(shutdowndelta >= secondsforvacates) {
				/* wait for a bit */
				int locdelta = (shutdowndelta - secondsforvacates);
				dprintf(D_ALWAYS, " EVENT_GO  in %d seconds\n",locdelta);
				m_mystate = EVENT_GO;
				//changeState(10, m_mystate); // 10 second wait for now...
				changeState(locdelta, m_mystate);
			} else {
				/* start now */
				dprintf(D_ALWAYS, " EVENT_GO now!\n");
				m_mystate = EVENT_GO;
				changeState(EVENT_NOW, m_mystate);
			}

			break;

		case EVENT_MAIN_WAIT:
			dprintf(D_ALWAYS, 
				"th_DoShutdown_States( no work - EVENT_MAIN_WAIT: )\n");
			break;

		case EVENT_GO:
			dprintf(D_ALWAYS, 
				"th_DoShutdown_States( <<<< EVENT_GO >>>> )\n");
			// add two minutes of work
			m_shutdownStart = time(NULL);
			loadCheckPointHash((int)m_newshutdownAdminRate * 120);
			do_checkpoint_shutdown(true);
			m_timerid_DoShutdown_States = 0; // next changeState will recreate
			break;

		case EVENT_DONE:
		{
			float 	megs_per_second = 0;
			int 	seconds = 0;
			char	line[300];
			dprintf(D_ALWAYS, 
				"th_DoShutdown_States( no work - EVENT_DONE: )\n");
			m_shutdownEnd = time(NULL);
			seconds = m_shutdownEnd - m_shutdownStart;
			megs_per_second = m_shutdownMegs/seconds;
			dprintf(D_ALWAYS, 
				"( EVENT_DONE: %f Megs Per Seconds<<%d/%d>>)\n"
				,megs_per_second,m_shutdownMegs,seconds);
			{
				dprintf(D_ALWAYS,"shutdownFast\n");
				char *tmp;

				delete m_lastShutdown;
				m_lastShutdown = new ClassAd();
				sprintf(line, "%s = %f", "LastShutdownRate", megs_per_second);
				m_lastShutdown->Insert(line);
				m_lastShutdown->dPrint(D_FULLDEBUG);

				tmp = param("SPOOL");
				if(tmp) {
					if(spoolClassAd(m_lastShutdown,"out") == 1) {
						dprintf(D_ALWAYS
							,"Failed to get/create initial classad\n");
					}
					free(tmp);
				}
			}
			break;
		}

		//case EVENT_INIT:
			//break;

		default:
			dprintf(D_ALWAYS, 
				"th_DoShutdown_States( Why Default Case )\n");
			break;
	}
}

/*

		Timers

*/

void
AdminEvent::th_DoShutdown( void )
{
	DCMaster *		d;
	//bool wantTcp = 	true;

	dprintf(D_ALWAYS,"th_DoShutdown\n");

	/* mark this timer as NOT active */
	m_timeridDoShutdown = -1;

	/* when we wake up we do the shutdown */
	dprintf(D_ALWAYS, 
		"<<<Time For Shutdown!!!!--%s--!!!>>>\n",m_shutdownTarget.Value());
	d = new DCMaster(m_shutdownTarget.Value());
	dprintf(D_ALWAYS,"daemon name is %s\n",d->name());
	dprintf(D_ALWAYS,"call Shutdown now.....\n");
	//d->sendMasterOff(wantTcp);
	delete d;
}

void 
AdminEvent::th_maintainCheckpoints( void )
{
	ClassAd *ad;
	int enteredcurrentstate;
	MyString state;
	MyString name;
	MyString jobid;
	MyString remoteuser;
	bool fFoundAd = false;
	bool fAllClear = true;

	/*
		for as long as we have jobs which have not
		been checkpointed, as checkpoints complete
		add more checkpoints into hash<m_JobNodes_su>.
	*/

	int timespent = 0;
	int gotads = 0;
	StartdStats *ss;

	// Where am I?
	dprintf(D_ALWAYS,"----<< th_maintainCheckpoints >>----\n");

	//standardUDisplay_StartdStats();
	m_JobNodes_su.startIterations();
	while(m_JobNodes_su.iterate(ss) == 1) {
		int timeNow = time(NULL);
		timespent = timeNow - ss->ckpttime;

		// check out this job first
		if(ss->ckptdone == 0) { 
			/* Still need to do this one.... */
			m_PollingStartdAds.Rewind();
			while( (ad = m_PollingStartdAds.Next()) ){
				m_PollingStartdAds.Delete(ad);
			}
			gotads = pollStartdAds(m_PollingStartdAds, ss->myaddress, ss->name);
			if( gotads == 1 ){
				dprintf(D_ALWAYS,"Failed to get ads for %s\n",ss->name);
				continue;
			} else {
				dprintf(D_FULLDEBUG,"Got ads for %s<%s>\n",ss->name,ss->jobid);
			}
			dprintf(D_FULLDEBUG,"pollAds:BM ----<<%s-%s-%s>>----\n",
				ss->myaddress, ss->name, ss->jobid);
			m_PollingStartdAds.Rewind();
			fFoundAd = false;
			while( (ad = m_PollingStartdAds.Next()) ){
				ad->LookupString( ATTR_NAME, name ); 
				ad->LookupString( ATTR_STATE, state ); 
				/* Considering...... */

				dprintf(D_FULLDEBUG,"pollAds:Considering ----<<%s-%ss>>----\n",
					name.Value(), state.Value());
				if(strcmp(name.Value(),ss->name) != MATCH) {
					dprintf(D_FULLDEBUG,"Chk other Ads Mismatch(%s-%s)\n",
						name.Value(),ss->name);
					continue;
				}

				if((strcmp(state.Value(),"Unclaimed") == MATCH) 
						||(strcmp(state.Value(),"Owner") == MATCH) 
						|| (strcmp(state.Value(),"Claimed") == MATCH)) 
				{
					dprintf(D_ALWAYS
						,"job size %d gone<state = %s>\n"
						,ss->imagesize,state.Value());
					if((strcmp(state.Value(),"Claimed") == MATCH)){
						// better be a different job
						ad->LookupString( ATTR_REMOTE_USER, remoteuser ); 
						ad->LookupString( ATTR_JOB_ID, jobid ); 
						dprintf(D_ALWAYS
							,"AD CLAIMED different job?-(%s)-<%s-%s-%s>-\n",
							remoteuser.Value(),name.Value(), state.Value()
							, jobid.Value());
					}

					// add more checkpoints
					break; /* The job is gone on its own */
				}

				ad->LookupString( ATTR_JOB_ID, jobid ); 

				dprintf(D_FULLDEBUG,"pollAds:AD ----<<%s-%s-%s>>----\n",
					name.Value(), state.Value(), jobid.Value());

				if( (strcmp(name.Value(), ss->name) == MATCH) 
						&& (strcmp(jobid.Value(), ss->jobid) == MATCH) 
						&& (ss->ckptdone == 0) ) 
				{
					fFoundAd = true;
					ss->state[0] = '\0';
					strcpy(ss->state,state.Value());
					ad->LookupInteger( ATTR_ENTERED_CURRENT_STATE, 
							enteredcurrentstate ); 
					if (ss->ckpttime > enteredcurrentstate ) {
						/* 
							still waiting on this node to checkpoint 
							and change state 
						*/
						dprintf(D_ALWAYS,
							"pollAds: Waiting on %s for %s in state %s\n",
							name.Value(),ss->jobid,ss->state);
						fAllClear = false;
					} else if( strcmp(ss->state,"Claimed") == MATCH) { 
						/* skip checks for this job for future benchmarks */
						dprintf(D_ALWAYS,
							"pollAds: CLAIMED??? %s for %s in state <%s>\n",
							name.Value(),ss->jobid,ss->state);
						fAllClear = false;
					} else if(strcmp(ss->state,"Preempting") != MATCH) {
							dprintf(D_ALWAYS,
								"ckptpt took <<%d>> for <<%s>>!<<STATE=%s>>\n",
								ss->ckptlength, ss->name,ss->state);
							SS_store(ss,timespent);
							loadCheckPointHash((ss->imagesize/1000));
					} else {
						dprintf(D_ALWAYS,
							"Waiting on %s for %s in state <<%s>>\n",
							name.Value(),ss->jobid,ss->state);
						fAllClear = false;
					}
				}
				m_PollingStartdAds.Delete(ad); /* no reason to keep it */
			}
			if(!fFoundAd) {
				/* The job is done and gone.... quit waiting.....*/
				SS_store(ss,timespent);
				loadCheckPointHash((ss->imagesize/1000));
				dprintf(D_ALWAYS,"Job done and gone <<%d>>\n",ss->imagesize);
			}
		} else {
			char *donestr;
			if(fAllClear) {
				donestr = "allclear";
			} else {
				donestr = "NOT-allclear";
			}
			dprintf(D_FULLDEBUG,"th_maintainCheckpoints <<< %s >>>\n",donestr);
		}
	}

	if(fAllClear){
		daemonCore->Cancel_Timer(m_timerid_maintainCheckpoints);

		dprintf(D_ALWAYS, 
			"th_maintainCheckpoints: m_timerid_DoShutdown_States is %d\n"
			,m_timerid_DoShutdown_States);

		benchmark_show_results();

		m_mystate = EVENT_DONE;
		changeState(EVENT_NOW, m_mystate);
	}
}

/*

		EVENT HANDLING METHODS

*/

int
AdminEvent::do_checkpoint_shutdown( bool initial )
{

	dprintf(D_ALWAYS, 
		"do_checkpoint_shutdown() starting checkpointing list and benchmark\n");

	/* 
		process the current standard universe jobs into a hash
		and then prepare a benchmark test. 
	*/

	m_timerid_maintainCheckpoints = daemonCore->Register_Timer(
		m_intervalCheck_maintainCheckpoints,
		m_intervalPeriod_maintainCheckpoints,
		(TimerHandlercpp)&AdminEvent::th_maintainCheckpoints,
		"AdminEvent::checkpoint_shutdown()", this );

	return 0;
}

/*

		MISC COMMANDS

*/

/*
	The following is a basic sort routine to sort the current
	standard universe jobs into longest running on their current claims 
	first. These are always our highest priority as they have the
	most to loose if shutdown without a checkpoint.

 	usage: classadlist.Sort( (SortFunctionType)ClaimRunTimeSort );

*/

int 
ClaimRunTimeSort(ClassAd *job1, ClassAd *job2, void *data)
{
    int claimruntime1=0, claimruntime2=0;

    job1->LookupInteger(ATTR_TOTAL_CLAIM_RUN_TIME, claimruntime1);
    job2->LookupInteger(ATTR_TOTAL_CLAIM_RUN_TIME, claimruntime2);
    if (claimruntime2 < claimruntime1) return 1;
    if (claimruntime2 > claimruntime1) return 0;
	return 0;
}

int
AdminEvent::empty_Hashes()
{
	StartdStats *ss;

	m_JobNodes_su.startIterations();
	while(m_JobNodes_su.iterate(ss) == 1) {
		delete ss;
	}

	m_JobNodes_su.clear(); /* clear our our sampling hash */
	return 0;
}

int
AdminEvent::pollStartdAds( ClassAdList &adsList, char *sinful, char *name )
{
	DCStartd *		d;
	bool fRes = true;

	//d = new DCStartd(name, NULL, sinful, NULL);
	d = new DCStartd(name, NULL);
	dprintf(D_FULLDEBUG,"daemon name is %s\n",d->name());
	dprintf(D_FULLDEBUG,"call getAds now.....\n");
	dprintf(D_FULLDEBUG, 
		"Want to get ads from here (%s) and this slot (%s)\n",sinful,name);
	fRes = d->getAds(adsList);
	if(fRes) {
		delete d;
		return(0);
	} else {
		delete d;
		return(1);
	}
}

int
AdminEvent::sendCheckpoint( char *sinful, char *name )
{
	DCStartd *		d;

	/* when we wake up we do the shutdown */
	d = new DCStartd(name, NULL, sinful, NULL);
	dprintf(D_FULLDEBUG,"daemon name is %s\n",d->name());
	dprintf(D_FULLDEBUG,"call checkpointJob now.....\n");
	dprintf(D_FULLDEBUG, 
		"Want to checkpoint to here (%s) and this slot (%s)\n",sinful,name);
	d->checkpointJob(name);
	delete d;
	return(0);
}

int
AdminEvent::sendVacateClaim( char *sinful, char *name )
{
	DCStartd *		d;
	ClassAd reply;
	//int timeout = -1;
	//bool fRes = true;

	/* when we wake up we do the shutdown */
	d = new DCStartd(name, NULL, sinful, NULL);
	d->vacateClaim(name);
	//fRes = d->releaseClaim(VACATE_GRACEFUL,&reply,timeout);
	//if(!fRes) {
		//dprintf(D_ALWAYS,
			//"d->releaseClaim(VACATE_GRACEFUL,&reply,timeout) FAILED\n");
	//}
	delete d;
	return(0);
}

int 
AdminEvent::benchmark_insert( float megspersec, int megs, int time, char *name)
{
	ClassAd *ad = new ClassAd();
	int mylength = 0;

	char line[100];
	sprintf(line, "%s = %s", "Name", name);
	ad->Insert(line);

	sprintf(line, "%s = %f", "MegsPerSec", megspersec);
	ad->Insert(line);

	sprintf(line, "%s = %d", "Megs", megs);
	ad->Insert(line);

	sprintf(line, "%s = %d", "Length", time);
	ad->Insert(line);

	m_CkptBenchMarks.Insert(ad);
	mylength = m_CkptBenchMarks.MyLength();
	return(0);
}

int
AdminEvent::benchmark_show_results()
{
	ClassAd *ad;
	float 	megspersec = 0;
	int 	megs = 0;
	int 	longesttime = 0;
	int		totalimagesz = 0;

	
	dprintf(D_FULLDEBUG,"***** Individual Ratings *****\n");
	m_CkptBenchMarks.Rewind();
	while( (ad = m_CkptBenchMarks.Next()) ){
		ad->LookupFloat( "MegsPerSec", megspersec );
		ad->LookupInteger( "Megs", megs );
		ad->LookupInteger( "Length", longesttime );
		dprintf(D_ALWAYS,"MegsPerSec = %f Megs = %d Longest = %d\n",
			megspersec, megs, longesttime);
		totalimagesz += megs;
	}

	dprintf(D_ALWAYS,"benchmark_show_results: Total Megs Image Size = %d\n",
		totalimagesz);

	return(0);
}

int AdminEvent::SS_store(StartdStats *ss, int duration)
{
	/* record this one */
	ss->ckptdone = 1;
	ss->ckptlength = duration;
	ss->ckptmegspersec = (float)((float)ss->ckptmegs/(float)ss->ckptlength);

	benchmark_insert(ss->ckptmegspersec, ss->ckptmegs, 
		duration, ss->name);

	dprintf(D_ALWAYS,"Store: MPS %f MEGS %d TIME %d\n",ss->ckptmegspersec,
		ss->ckptmegs, duration);

	return(0);
}

/*

		PROCESSING ROUTINES

*/

int
AdminEvent::process_ShutdownTime( char *req_time )
{
	// Crunch time into time_t format via str to tm
	struct tm tm;
	struct tm *tmnow;
	char *res;
	m_timeNow = time(NULL);

	dprintf(D_FULLDEBUG, "process_ShutdownTime: timeNow is %ld\n",m_timeNow);

	// make a tm with info for now and reset
	// secs, day and hour from the next scan of the req_time`
	tmnow = localtime(&m_timeNow);

	// find secs, hour and minutes
	res = strptime(req_time,"%H:%M:%S",&tm);
	if(res != NULL) {
		dprintf(D_FULLDEBUG, 
			"Processing Shutdown Time String<<LEFTOVERS--%s-->>\n",res);
		//return(-1);
	}

	// Get today into request structure
	tm.tm_mday = tmnow->tm_mday;
	tm.tm_mon = tmnow->tm_mon;
	tm.tm_year = tmnow->tm_year;
	tm.tm_wday = tmnow->tm_wday;
	tm.tm_yday = tmnow->tm_yday;
	tm.tm_isdst = tmnow->tm_isdst;

	dprintf(D_ALWAYS, "Processing Shutdown Time secs <%d> mins <%d> hrs <%d>\n",
		tm.tm_sec,tm.tm_min,tm.tm_hour);
	// Get our time_t value
	m_newshutdownTime = mktime(&tm);
	m_shutdownDelta = m_newshutdownTime - m_timeNow;
	dprintf(D_ALWAYS, "New Shutdown Time <%d> Shutdown Delta <%d>\n",
						(int)m_newshutdownTime, (int)m_shutdownDelta);
	
	return(0);
}

int
AdminEvent::FetchAds_ByConstraint( const char *constraint )
{
	CondorError errstack;
	CondorQuery *query;
    QueryResult q;
	ClassAd *ad;
	DCCollector* pool = NULL;
	AdTypes     type    = (AdTypes) -1;

	MyString machine;
	MyString state;
	MyString sinful;
	MyString name;
	MyString remoteuser;

	int jobuniverse = -1;
	int totalclaimruntime = -1;

	pool = new DCCollector( "" );

	if( !pool->addr() ) {
		dprintf (D_ALWAYS, 
			"Getting Collector Object Error:  %s\n",pool->error());
		return(1);
	}

	// fetch the query

	// we are looking for starter ads
	type = STARTD_AD;

	// instantiate query object
	if( !(query = new CondorQuery (type))) {
		dprintf (D_ALWAYS, 
			"Getting Collector Query Object Error:  Out of memory\n");
		return(1);
	}

	dprintf(D_FULLDEBUG, "Processing Shutdown constraint String<<%s>>\n",
		constraint);

	query->addORConstraint( constraint );

	q = query->fetchAds( m_collector_query_ads, pool->addr(), &errstack);

	if( q != Q_OK ){
		dprintf(D_ALWAYS, "Trouble fetching Ads with<<%s>><<%d>>\n",
			constraint,q);
		delete query;
		delete pool;
		return(1);
	}

	if( m_collector_query_ads.Length() <= 0 ){
		dprintf(D_ALWAYS, "Found no ClassAds matching <<%s>> <<%d results>>\n",
			constraint,m_collector_query_ads.Length());
	} else {
		dprintf(D_FULLDEBUG, "Found <<%d>> ClassAds matching <<%s>>\n",
			m_collector_query_ads.Length(),constraint);
	}

	// output result
	// we always fill the sorted class ad lists with the result of the query
	m_claimed_standard.Rewind();
	m_collector_query_ads.Rewind();
	while( (ad = m_collector_query_ads.Next()) ){
		ad->LookupString( ATTR_MACHINE, machine );
		ad->LookupString( ATTR_STATE, state );
		ad->LookupString( ATTR_MY_ADDRESS, sinful );
		ad->LookupString( ATTR_REMOTE_USER, remoteuser );
		ad->LookupInteger( ATTR_JOB_UNIVERSE, jobuniverse );
		ad->LookupInteger( ATTR_TOTAL_CLAIM_RUN_TIME, totalclaimruntime );
		ad->LookupString( ATTR_NAME, name ); 

		if( ! machine.Value() ) {
			dprintf(D_ALWAYS, "malformed ad????\n");
			continue;
		}

		if(jobuniverse == CONDOR_UNIVERSE_STANDARD) {
			m_claimed_standard.Insert(ad);
		}

		m_collector_query_ads.Delete(ad);
	}

	// sort list with oldest standard universe jobs first
	m_claimed_standard.Sort( (SortFunctionType)ClaimRunTimeSort );

	delete pool;
	delete query;
	return(0);
}

int
AdminEvent::totalRunningJobs()
{
	ClassAd *ad;

	MyString sinful;
	MyString clientmachine;
	MyString jobid;	
	MyString ckptsrvr;

	int		imagesize;	
	int 	totalimagesize = 0;

	//dprintf(D_ALWAYS,"totalRunningJobs\n");

	//dprintf(D_ALWAYS,"The following were assigned claimed standard U\n");
	m_claimed_standard.Rewind();
	while( (ad = m_claimed_standard.Next()) ){
		imagesize = 0;
		ad->LookupString( ATTR_MY_ADDRESS, sinful ); 
		//dprintf(D_ALWAYS,"Sinful %s\n",sinful.Value());
		ad->LookupString( ATTR_CLIENT_MACHINE, clientmachine ); 
		ad->LookupString( ATTR_JOB_ID, jobid ); 
		ad->LookupString( ATTR_CKPT_SERVER, ckptsrvr ); 
		ad->LookupInteger( ATTR_IMAGE_SIZE, imagesize ); 
		if(imagesize > 0) {
			totalimagesize += (imagesize/1000);
		}
	}

	dprintf(D_ALWAYS,"^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
	dprintf(D_ALWAYS,
		"Total of jobs from Active Constraint <<<<<%d megs>>>>>\n",
		totalimagesize);
	dprintf(D_ALWAYS,"^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
	return(totalimagesize);
}

int
AdminEvent::loadCheckPointHash( int megs )
{
	//standardUDisplay();
	standardUProcess( megs, true );
	return 0;
}

int
AdminEvent::standardUProcess( int batchsz, bool vacate )
{
	ClassAd *ad;

	MyString machine;
	MyString sinful;
	MyString name;
	MyString state;
	MyString activity;
	MyString clientmachine;
	MyString jobid;	
	MyString ckptsrvr;

	int		slotid;	
	int		jobuniverse;	
	int		jobstart;	
	int		imagesize;	
	int		lastperiodiccheckpoint;	
	int 	remotetimeNow;
	int 	thisbatch=0;	/* processed this call */

	time_t timeNow = time(NULL);
	int     testtime = 0;

	StartdStats *ss;

	m_claimed_standard.Rewind();
	while( (ad = m_claimed_standard.Next()) ){
		slotid = -1;
		ad->LookupString( ATTR_MACHINE, machine );
		ad->LookupString( ATTR_MY_ADDRESS, sinful ); 
		ad->LookupString( ATTR_NAME, name ); 
		HashKey		namekey(name.Value());
		ad->LookupString( ATTR_STATE, state ); 
		ad->LookupString( ATTR_ACTIVITY, activity ); 
		ad->LookupString( ATTR_CLIENT_MACHINE, clientmachine ); 
		ad->LookupString( ATTR_JOB_ID, jobid ); 
		ad->LookupString( ATTR_CKPT_SERVER, ckptsrvr ); 
		ad->LookupInteger( ATTR_JOB_START, jobstart ); 
		ad->LookupInteger( ATTR_JOB_UNIVERSE, jobuniverse ); 
		ad->LookupInteger( ATTR_IMAGE_SIZE, imagesize ); 
		if (param_boolean("ALLOW_VM_CRUFT", true)) {
			if (!ad->LookupInteger(ATTR_SLOT_ID, slotid)) {
				ad->LookupInteger(ATTR_VIRTUAL_MACHINE_ID, slotid);
			}
		} else {
			ad->LookupInteger(ATTR_SLOT_ID, slotid);
		}
		ad->LookupInteger( "MonitorSelfTime", remotetimeNow ); 
		ad->LookupInteger( ATTR_LAST_PERIODIC_CHECKPOINT, 
			lastperiodiccheckpoint ); 
		dprintf(D_FULLDEBUG,"lookup name %s Jobid %s\n"
			,name.Value(),jobid.Value());
		if(m_JobNodes_su.lookup(namekey,ss) < 0) {
			//dprintf(D_ALWAYS,"Must hash name %s \n",name.Value());
			ss = new StartdStats(name.Value(), jobuniverse, imagesize, 
				lastperiodiccheckpoint);
			ss->slotid = slotid;
			ss->jobstart = jobstart;
			ss->remotetime = timeNow - remotetimeNow;
			testtime = timeNow + ss->remotetime; /* adjust by remote diff */
			ss->ckpttime = testtime;
			ss->ckptlength = 0;
			ss->ckptmegs = (imagesize/1000);

			strcpy(ss->clientmachine, clientmachine.Value());
			strcpy(ss->state, state.Value());
			strcpy(ss->activity, activity.Value());
			strcpy(ss->myaddress, sinful.Value());
			strcpy(ss->jobid, jobid.Value());
			strcpy(ss->ckptsrvr, ckptsrvr.Value());


			m_JobNodes_su.insert(namekey,ss);
			thisbatch += (imagesize/1000);
			if(vacate) {
				//sendCheckpoint(ss->myaddress,ss->name);
				ss->ckpttime = timeNow;
				sendVacateClaim(ss->myaddress,ss->name);
			}
			dprintf(D_FULLDEBUG,"Batch size to %d\n",thisbatch);
		} else {
			dprintf(D_ALWAYS,
				"Why is %s already in hash table??\n",name.Value());
		}

		m_claimed_standard.Delete(ad);
		if(batchsz > 0) {
			// we only want to enable a limited amount at this time
			if(thisbatch >= batchsz) {
				dprintf(D_ALWAYS,
					"standardUProcess: Added batch size of %d((vacate=%d))\n",
					thisbatch,vacate);
				// we are done
				return(0);
			}
		}
	}
	return(0);
}

/*

		DISPLAY ROUTINES

*/

int
AdminEvent::standardUDisplay_StartdStats( )
{
	dprintf(D_FULLDEBUG,"standardUDisplay_StartdStats\n");

	StartdStats *ss;
	m_JobNodes_su.startIterations();
	while(m_JobNodes_su.iterate(ss) == 1) {
		dprintf(D_ALWAYS,
		"***************<<standardUDisplay_StartdStats>>*****************\n");
		dprintf(D_ALWAYS,"JobId %s Name %s SZ %d\n",ss->jobid,ss->name,
			ss->imagesize);
		dprintf(D_FULLDEBUG,"State %s\n",ss->state);
		dprintf(D_FULLDEBUG,"Activity %s\n",ss->activity);
		dprintf(D_FULLDEBUG,"ClientMachine %s\n",ss->clientmachine);
		dprintf(D_FULLDEBUG,"MyAddress %s\n",ss->myaddress);
		dprintf(D_FULLDEBUG,"Universe %d\n",ss->universe);
		dprintf(D_FULLDEBUG,"Jobstart %d\n",ss->jobstart);
		dprintf(D_FULLDEBUG,"LastCheckPoint %d\n",ss->lastcheckpoint);
		dprintf(D_ALWAYS,"SlotId %d\n",ss->slotid);
		dprintf(D_FULLDEBUG,"--------------------------------------\n");
		dprintf(D_FULLDEBUG,"CkptTime %d\n",ss->ckpttime);
		dprintf(D_FULLDEBUG,"CkptLength %d\n",ss->ckptlength);
		dprintf(D_FULLDEBUG,"CkptGroup %d\n",ss->ckptgroup);
		dprintf(D_ALWAYS,"CkptDone %d\n",ss->ckptdone);
		dprintf(D_FULLDEBUG,"CkptMegs %d CKPTSRVR %s\n",ss->ckptmegs,
			ss->ckptsrvr);
	}
	dprintf(D_FULLDEBUG,"**************************************\n");
	return(0);
}

int
AdminEvent::standardUDisplay()
{
	ClassAd *ad;
	MyString machine;
	MyString sinful;
	MyString name;

	dprintf(D_ALWAYS,"The following were assigned claimed standard U\n");
	m_claimed_standard.Rewind();
	while( (ad = m_claimed_standard.Next()) ){
		ad->LookupString( ATTR_MACHINE, machine );
		ad->LookupString( ATTR_MY_ADDRESS, sinful );
		ad->LookupString( ATTR_NAME, name );
		if( ! machine.Value() ) {
			dprintf(D_ALWAYS, "malformed ad????\n");
			continue;
		} else {
			dprintf(D_ALWAYS, 
				"Found <<%s>> machine matching <<%s>> Standard SORTED!!!!\n",
				machine.Value(),m_shutdownConstraint.Value());
			//ad->dPrint( D_ALWAYS );
		}
	}
	return(0);
}
