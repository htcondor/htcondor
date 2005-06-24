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

#ifndef PARALLELSHADOW_H
#define PARALLELSHADOW_H

#include "condor_common.h"
#include "baseshadow.h"
#include "mpiresource.h"
#include "list.h"


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
	void init( ClassAd* job_ad, const char* schedd_addr );

		/** Shadow should spawn a new starter for this job.
		 */
	void spawn( void );

		/** Shadow should attempt to reconnect to a disconnected
			starter that might still be running for this job.  
			TODO: this does not yet work!
		 */
	void reconnect( void );

	bool supportsReconnect( void );


	void shutDown( int exitReason );

		/** Handle job removal. */
	int handleJobRemoval( int sig );

	int updateFromStarter(int command, Stream *s);

	struct rusage getRUsage( void );

	int getImageSize( void );

	int getDiskUsage( void );

	int getExitReason( void );

	float bytesSent( void );
	float bytesReceived( void );

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
	virtual void emailTerminateEvent( int exitReason );

		/** Do all work to cleanup before this shadow can exit.  To
			cleanup an MPI job, we've got to kill all our starters.
		*/
	virtual void cleanUp( void );

		/** Do a graceful shutdown of this computation.
			Unfortunately, there's no such thing as a graceful
			shutdown of an MPI job, so we're just going to have to
			call cleanUp() in the MPI case. 
		*/
	virtual void gracefulShutDown( void );

	virtual void resourceBeganExecution( RemoteResource* rr );

	virtual void resourceReconnected( RemoteResource* rr );

	virtual void logDisconnectedEvent( const char* reason );

 protected:

	virtual void logReconnectedEvent( void );

	virtual void logReconnectFailedEvent( const char* reason );

 private:

        /** After the schedd claims a resource, it puts it in a queue
            and then sends us a RESOURCE_AVAILABLE signal.  Upon
            receipt of that signal (it's registered in init()), we
            enter this function */
    int getResources( void );

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

		/** A complex function that deals with the end of an MPI
			job.  It has two functions: 1) figure out if all the 
			resources should be told to kill themselves and 
			2) return TRUE if every resource is dead. 
		    @param exitReason The job exit reason. */
	int shutDownLogic( int& exitReason );

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
	ExtArray<MpiResource *> ResourceList;

		/** Replace $(NODE) with the proper node number */
	void replaceNode ( ClassAd *ad, int nodenum );
	
	int info_tid;	// DC id for our timer to get resource info 

	bool is_reconnect; 
};


#endif /* PARALLELSHADOW_H */
