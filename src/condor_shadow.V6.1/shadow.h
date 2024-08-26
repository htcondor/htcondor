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


#ifndef SHADOW_H
#define SHADOW_H

#include "condor_common.h"
#include "baseshadow.h"
#include "remoteresource.h"

class ShadowHookMgr;

/** This class is the implementation for the shadow.  It is 
	called UniShadow because:
	<ul>
	 <li>It's not named CShadow (sorry, Todd).
	 <li>It deals with <b>one</b> remote host.
	 <li>Calling it 'Shadow' isn't good, with all the Shadow flavors.
	 <li>I suck at naming things.
	</ul>

	<p>
	Much of the base functionality of Shadowness can be found in
	BaseShadow.  This class uses one instance of RemoteResource to
	represent the remote job.

	<p>
	Based heavily on code by Todd Tannenbaum.
	@see RemoteResource
	@author Mike Yoder
*/
class UniShadow : public BaseShadow
{
 public:

		/// Constructor.  Only makes a new RemoteResource.
	UniShadow();

		/// Destructor, it's virtual
	virtual ~UniShadow();

		/** This is the init() method that gets called upon startup.

			Does the following:
			<ul>
			 <li>Checks some parameters
			 <li>Sets up the remote Resource
			 <li>Calls BaseShadow::init()
			 <li>Requests the remote resource
			 <li>Makes a log execute event
			 <li>Registers the RemoteResource's claimSock
			</ul>
			The parameters passed are all gotten from the 
			command line and should be easy to figure out.
		*/
	void init( ClassAd* job_ad, const char* schedd_addr, const char *xfer_queue_contact_info );
	
		/** Shadow should spawn a new starter for this job.
		 *  May be asynchronous if there's a shadow hook defined.
		 */
	void spawn( void );

		/**
		 * Callback after shadow hook has finished.
		 */
	void spawnFinish();

		/**
		 * Invoked when the job hook times out
		 */
	void hookTimeout( int timerID = -1 );
	void hookTimerCancel();

		/** Shadow should attempt to reconnect to a disconnected
			starter that might still be running for this job.  
		 */
	void reconnect( void );

	bool supportsReconnect( void );

	/**
	 * override to allow starter+shadow to gracefully exit 
	 */
	virtual void removeJob( const char* reason );
	
	/**
	 * override to allow starter+shadow to gracefully exit 
	 */
	virtual void holdJob( const char* reason, int hold_reason_code, int hold_reason_subcode );
	
		/**
		 */
	int handleJobRemoval(int sig);

		/// Log an execute event to the UserLog
	void logExecuteEvent( void );

		/** Return the exit reason for this shadow's job.  Since we've
			only got 1 RemoteResource, we can just return its value.
		*/
	int getExitReason( void );

		/** Return true if the startd is not accepting more jobs on
			this claim.
		*/
	bool claimIsClosing( void );

		/* The number of bytes transferred from the perspective of
		 * the shadow (NOT the starter/job).
		 */
	float bytesSent();
	float bytesReceived();
	void getFileTransferStats(ClassAd &upload_stats, ClassAd &download_stats);
	void getFileTransferStatus(FileTransferStatus &upload_status,FileTransferStatus &download_status);

	int updateFromStarterClassAd(ClassAd* update_ad);

	struct rusage getRUsage( void );

	int64_t getImageSize( int64_t & mem_usage, int64_t & rss, int64_t & pss );

	int64_t getDiskUsage( void );

	bool exitedBySignal( void );

	int exitSignal( void );

	int exitCode( void );

		/** This function is specifically used for spawning MPI jobs.
			So, if for some bizzare reason, it gets called for a
			non-MPI shadow, we should return falure.
		*/
	bool setMpiMasterInfo( char* ) { return false; };

		/** If desired, send the user email now that this job has
			terminated.  This has all the job statistics from the run,
			and lots of other useful info.
		*/
	virtual void emailTerminateEvent( int exitReason, 
					update_style_t kind = US_NORMAL );

	// Record the file transfer state changes.
	virtual void recordFileTransferStateChanges( ClassAd * jobAd, ClassAd * ftAd );

		/** Do all work to cleanup before this shadow can exit.  We've
			only got 1 RemoteResource to kill the starter on.
		*/
	virtual void cleanUp( bool graceful=false );

		/** Do a graceful shutdown of the remote starter */
	virtual void gracefulShutDown( void );

	virtual void resourceBeganExecution( RemoteResource* rr );

	virtual void resourceDisconnected( RemoteResource* rr );

	virtual void resourceReconnected( RemoteResource* rr );

	virtual void logDisconnectedEvent( const char* reason );

	virtual bool getMachineName( std::string &machineName );
	
	/**
	 * Handle the situation where the job is to be suspended
	 */
	virtual int JobSuspend(int sig);
	
	/**
	 * Handle the situation where the job is to be continued.
	 */
	virtual int JobResume(int sig);

	virtual void exitAfterEvictingJob( int reason );
	virtual bool exitDelayed( int &reason );

	void exitLeaseHandler( int timerID = -1 ) const;

	ClassAd *getJobAd() { return remRes ? remRes->getJobAd() : nullptr; };
 protected:

	virtual void logReconnectedEvent( void );

	virtual void logReconnectFailedEvent( const char* reason );

 private:
	RemoteResource *remRes;
	std::unique_ptr<ShadowHookMgr> m_hook_mgr;
	int m_exit_hook_timer_tid{-1};

	int delayedExitReason;

	void requestJobRemoval();
};

#endif
