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

#ifndef PARALLELSHADOW_H
#define PARALLELSHADOW_H

#include "condor_common.h"
#include "baseshadow.h"
#include "parallelresource.h"
#include "list.h"

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
			 <li>Talks to the schedd to get all the hosts and capabilities
			       we'll need for this parallel job
			 <li>Calls BaseShadow::init()
			 <li>Requests all the remote resources
			 <li>Makes a log execute event
			 <li>Registers the RemoteResource's claimSock
			</ul>
			The parameters passed are all gotten from the 
			command line and should be easy to figure out.
		 */
	void init( ClassAd *jobAd, char schedd_addr[], char host[], 
			   char capability[], char cluster[], char proc[]);

		/** Shut down properly.  We have parallel-specific logic in this
			version which decides if we're really ready to shutdown or
			not.  Once it's going to really shutdown, it just calls
			the BaseShadow version to do the real work (which is the
			same in both cases).
			@param exitReason The reason this parallel job exited.
		*/
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

		/** If desired, send the user email now that this job has
			terminated.  For parallel jobs, we print out all the hosts
			where the job ran, and any other useful info.
		*/
	virtual void emailTerminateEvent( int exitReason );

		/** Do all work to cleanup before this shadow can exit.  To
			cleanup an parallel job, we've got to kill all our starters,
			and remove the job ads file, if we're using one.
		*/
	virtual void cleanUp( void );

		/** Do a graceful shutdown of this computation.
			Unfortunately, there's no such thing as a graceful
			shutdown of an parallel job, so we're just going to have to
			call cleanUp() in the parallel case. 
		*/
	virtual void gracefulShutDown( void );

	virtual void resourceBeganExecution( RemoteResource* rr );

		/** This function is specifically used for spawning MPI jobs.
			So, if for some bizzare reason, it gets called for a
			non-MPI shadow, we should return falure.
		*/
	bool setMpiMasterInfo( char* ) { return false; };

 private:

		/** This reads the job ad file back in, creating new job ads for
			RemoteResource[node] and those after it. See the comment below for
			more information. */
	int readJobAdFile( int node );

		/** This runs the user-specified shadow side parallel startup script.
			It is run on two occasions:
			- before starting the master node (called by startMaster)
			- before starting each comrade node (called by startComrade, which
			  is called by the sneaky rsh)
			On each occasion, the script is passed as its first argument the
			name of a file containing the job ads of all the resources. The
			script may modify this file. The file will then be read back in
			and new (potentially different) job ads will be created from its
			contents. When the script is called in the second context, its
			second argument is its node number, n. Its third through last
			arguments are the arguments received by the special rsh. In this
			context, it is only meaningful for the script to modify the nth
			through last ads in the file. */
	int runParallelStartupScript( char* rshargs );

		/** This is registered as the reaper for the script. It just continues
			the process from after the script onwards.
			@param pid the pid that died
			@param exit_status the exit status of the dead pid
		*/
	int postScript(int pid, int exit_status);

        /** After the schedd claims a resource, it puts it in a queue
            and then sends us a RESOURCE_AVAILABLE signal.  Upon
            receipt of that signal (it's registered in init()), we
            enter this function */
    int getResources( void );

        /** When we've got all the resources we need, we enter this
            function, which starts the parallel job */
    void startMaster();

        /** We will be given the args to start the parallel process by 
            the sneaky rsh program.  Start a Comrade with them */
    int startComrade(int cmd, Stream *s);

		/** This function is shared by all the different methods that
			spawn nodes (root vs. comrade and rsh vs. non-rsh).
			@param rr Pointer to the RemoteResource to spawn
		*/
	void spawnNode( ParallelResource* rr );

		/** A complex function that deals with the end of an PARALLEL
			job.  It has two functions: 1) figure out if all the 
			resources should be told to kill themselves and 
			2) return TRUE if every resource is dead. 
		    @param exitReason The job exit reason. */
	int shutDownLogic( int& exitReason );
	
		/** The reaper id of postScript */
	int postScript_rid;

        /** The name of the job ad file we give to the user supplied script */
    char jobadfile[_POSIX_PATH_MAX];

        /** The name of the user supplied script that is run shadow side */
	char* parallelscriptshadow;

        /** The number of the next resource to start...when in start mode */
    int nextResourceToStart;

		/** The number of nodes for this job */
	int numNodes;

        /** TRUE if we're in the process of shutting down a job. */
    bool shutDownMode;

		/** Used to determine actual exit conditions...*/
	int actualExitReason;
	
		/** Find the ParallelResource corresponding to the given node
			number.
			@param node Which node you're looking for.
			@return The ParallelResource object, or NULL if not found. 
		*/
	ParallelResource* findResource( int node );

		// the list of remote (parallel) resources
		// Perhaps use STL soon.
	ExtArray<ParallelResource *> ResourceList;

		/** Replace $(NODE) with the proper node number */
	void replaceNode ( ClassAd *ad, int nodenum );
	
	int info_tid;	// DC id for our timer to get resource info 
};


#endif /* PARALLELSHADOW_H */
