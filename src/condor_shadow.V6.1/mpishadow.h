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


#ifndef MPISHADOW_H
#define MPISHADOW_H

#include "condor_common.h"
#include "baseshadow.h"
#include "mpiresource.h"
#include "list.h"

#if defined( WIN32 )
#define MPI_USES_RSH FALSE
#else
#define MPI_USES_RSH TRUE
#endif

/** This is the MPI Shadow class.  It acts as a shadow for an MPI
	job submitted to Condor.<p>

	The basic trick is this.  The MPICH job uses the "ch_p4" device, 
	which is the lower-layer of communication.  The ch_p4 device uses
	rsh in the "master" to start up nodes on other machines.  We 
	(condor) place our own version of rsh in the $path of the master, 
	(which has been started up on a machine by our starter...) and 
	this sneaky rsh then calls the shadow, telling it the arguments 
	that it was given.  This happens for every "slave" mpi process 
	we wish to start up.  (Being more liberal, we call slaves "comrades")
	*grin*.  We fire up a comrade on each machine we've got claimed, 
	and the mpi job is then off to the races...

	Deciding when an MPI job is finished is a bit of work.  The 
	individual nodes can exit at any time.  If a node SIGSEGVs, p4
	catches that and makes all nodes exit with a status of 1.  This
	is hard for us to decipher, but we print a warning if all the nodes
	exit with status 1.  If one of our supposedly "dedicated" nodes
	goes away, we kill the entire job.

	@author Mike Yoder
*/
class MPIShadow : public BaseShadow
{
 public:

		/// Constructor
	MPIShadow();

		/// Destructor
	virtual ~MPIShadow();

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
			TODO: this does not yet work for MPI universe!
		 */
	void reconnect( void );

	bool supportsReconnect( void );


		/** Shut down properly.  We have MPI-specific logic in this
			version which decides if we're really ready to shutdown or
			not.  Once it's going to really shutdown, it just calls
			the BaseShadow version to do the real work (which is the
			same in both cases).
			@param exitReason The reason this mpi job exited.
		*/
	void shutDown( int exitReason );

		/** Handle job removal. */
	int handleJobRemoval( int sig );

	int updateFromStarter(int command, Stream *s);
	int updateFromStarterClassAd(ClassAd* update_ad);

	struct rusage getRUsage( void );

	int getImageSize( void );

	int getDiskUsage( void );

	int getExitReason( void );

	bool claimIsClosing( void );

	float bytesSent( void );
	float bytesReceived( void );

	bool exitedBySignal( void );

	int exitSignal( void );

	int exitCode( void );

		/** Record the IP and port where the MPI master is running for
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
			cleanup an MPI job, we've got to kill all our starters,
			and remove the procgroup file, if we're using one.
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

#if (MPI_USES_RSH) 
        /** We will be given the args to start the mpi process by 
            the sneaky rsh program.  Start a Comrade with them */
    int startComrade(int cmd, Stream *s);

        /** Does necessary things to the args obtained from the 
            sneaky rsh. */
    void hackComradeAd( char *comradeArgs, ClassAd *ad );

        /** Pretty simple: takes the args, adds a -p4gp ..., puts
            the args back in. */
    void hackMasterAd( ClassAd *ad );
    
#else
		/** Once we have the IP and port of the master, we can spawn
			all the comrade nodes at once.  
		*/
	void spawnAllComrades( void );

		/** Adds the necessary variables to the environment of any
			node in the MPI computation so it can properly execute. 
			@param ad Pointer to the ClassAd to modify
		 */
	bool modifyNodeAd( ClassAd* );

#endif /* ! MPI_USES_RSH */

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

#if ! MPI_USES_RSH
	char* master_addr;   // string containing the IP and port where
		                 // the MPI master node is running.

	char* mpich_jobid;   // job ID string, we use
                         // "submit_host.cluster.proc" 

#endif

};


#endif /* MPISHADOW_H */
