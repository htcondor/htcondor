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


#ifndef PARALLELSHADOW_H
#define PARALLELSHADOW_H

#include "condor_common.h"
#include "baseshadow.h"
#include "mpiresource.h"



/** This is the Parallel Shadow class.  It acts as a shadow for
	Condor's parallel jobs.<p>

*/
class ParallelShadow : public BaseShadow
{
 public:

		/// Constructor
	ParallelShadow();

		/// Destructor
	virtual ~ParallelShadow();

		/**	Does the following:
			<ul>
			 <li>Checks some parameters
			 <li>Talks to the schedd to get all the hosts and ClaimIds
			       we'll need for this mpi job
			 <li>Calls BaseShadow::init()
			 <li>Requests all the remote resources
			 <li>Makes a log execute event
			 <li>Registers the RemoteResource's claimSock
			</ul>
			The parameters passed are all gotten from the 
			command line and should be easy to figure out.
		 */
	void init( ClassAd* job_ad, const char* schedd_addr, const char *xfer_queue_contact_info );

		/** Shadow should spawn a new starter for this job.
		 */
	void spawn( void );

		/** Shadow should attempt to reconnect to a disconnected
			starter that might still be running for this job.  
			TODO: this does not yet work!
		 */
	void reconnect( void );

	bool supportsReconnect( void );

	virtual bool shouldAttemptReconnect(RemoteResource *r);

	void shutDown( int exitReason, const char* reason_str, int reason_code=0, int reason_subcode=0 );

	/**
	 * override to allow starter+shadow to gracefully exit
	 */
	virtual void holdJob( const char* reason, int hold_reason_code, int hold_reason_subcode );

		/** Handle job removal. */
	int handleJobRemoval( int sig );

	int updateFromStarterClassAd(ClassAd* update_ad);

	struct rusage getRUsage( void );

	int64_t getImageSize( int64_t & memory_usage, int64_t & rss, int64_t & pss );

	int64_t getDiskUsage( void );

	int getExitReason( void );

	bool claimIsClosing( void );

	float bytesSent( void );
	float bytesReceived( void );
	void getFileTransferStats(ClassAd &upload_stats, ClassAd &download_stats);
	void getFileTransferStatus(FileTransferStatus &upload_status,FileTransferStatus &download_status);

	bool exitedBySignal( void );

	int exitSignal( void );

	int exitCode( void );

		/** Record the IP and port where the master is running for
			this computation.  Once we get this info, we can spawn all
			the workers, so start the ball rolling on that, too.
			@param str A string containing the IP and port, separated
			by a semicolon (e.g. "128.105.102.46:2342")
			@return Always return true, since we're an MPI shadow
		*/
	bool setMpiMasterInfo( char* str );

		/** If desired, send the user email now that this job has
			terminated.  For MPI jobs, we print out all the hosts
			where the job ran, and any other useful info.
		*/
	virtual void emailTerminateEvent( int exitReason, 
					update_style_t kind = US_NORMAL );

		/** Do all work to cleanup before this shadow can exit.  To
			cleanup an MPI job, we've got to kill all our starters.
		*/
	virtual void cleanUp( bool graceful=false );

		/** Do a graceful shutdown of this computation.
			Unfortunately, there's no such thing as a graceful
			shutdown of an MPI job, so we're just going to have to
			call cleanUp() in the MPI case. 
		*/
	virtual void gracefulShutDown( void );

	virtual void resourceBeganExecution( RemoteResource* rr );

	virtual void resourceDisconnected( RemoteResource* ) { }

	virtual void resourceReconnected( RemoteResource* rr );

	virtual void logDisconnectedEvent( const char* reason );

	virtual void recordFileTransferStateChanges( ClassAd * /* jobAd */, ClassAd * /* ftAd */ ) { }

	virtual bool updateJobAttr(const char*, const char*, bool log=false);

	virtual bool updateJobAttr(const char*, int64_t, bool log=false);
	
	/**
	 * Handle the situation where the job is to be suspended
	 */
	virtual int JobSuspend(int sig);
	
	/**
	 * Handle the situation where the job is to be continued.
	 */
	virtual int JobResume(int sig);

 protected:

	virtual void logReconnectedEvent( void );

	virtual void logReconnectFailedEvent( const char* reason );


 private:

	enum ParallelShutdownPolicy {
		WAIT_FOR_NODE0, WAIT_FOR_ALL, WAIT_FOR_ANY
	};
        /** After the schedd claims a resource, it puts it in a queue
            and then sends us a RESOURCE_AVAILABLE signal.  Upon
            receipt of that signal (it's registered in init()), we
            enter this function */
    void getResources( int timerID = -1 );

        /** When we've got all the resources we need, we enter this
            function, which starts the mpi job */
    void startMaster();

		/** Once we have the IP and port of the master, we can spawn
			all the comrade nodes at once.  
		*/
	void spawnAllComrades( void );

		/** Adds the necessary variables to the environment of any
			node in the MPI computation so it can properly execute. 
			@param ad Pointer to the ClassAd to modify
		 */
	bool modifyNodeAd( ClassAd* );

		/** This function is shared by all the different methods that
			spawn nodes (root vs. comrade and rsh vs. non-rsh).
			@param rr Pointer to the RemoteResource to spawn
		*/
	void spawnNode( MpiResource* rr );

        /** The number of the next resource to start...when in start mode */
    int nextResourceToStart;

		/** The number of nodes for this job */
	int numNodes;

        /** TRUE if we're in the process of shutting down a job. */
    bool shutDownMode;

		/** Used to determine actual exit conditions...*/
	int actualExitReason;
	
		/** Find the MpiResource corresponding to the given node
			number.
			@param node Which node you're looking for.
			@return The MpiResource object, or NULL if not found. 
		*/
	MpiResource* findResource( int node );

		// the list of remote (mpi) resources
		// Perhaps use STL soon.
	std::vector<MpiResource *> ResourceList;

		/** Replace $(NODE) with the proper node number */
	void replaceNode ( ClassAd *ad, int nodenum );
	
	int info_tid;	// DC id for our timer to get resource info 

	bool is_reconnect; 

	ParallelShutdownPolicy shutdownPolicy;
};


#endif /* PARALLELSHADOW_H */
