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

#ifndef MPISHADOW_H
#define MPISHADOW_H

#include "condor_common.h"
#include "baseshadow.h"
#include "mpiresource.h"
#include "list.h"

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
			 <li>Talks to the schedd to get all the hosts and capabilities
			       we'll need for this mpi job
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

		/** Shut down properly.  Send email to the user if requested.
			Calls DC_Exit(), therefore it doesn't return.
			@param exitReason The reason this mpi job exited.
			@param exitStatus The Status upon exit. 
		*/
	void shutDown( int exitReason, int exitStatus );

		/** Handle job removal. */
	int handleJobRemoval( int sig );

	int updateFromStarter(int command, Stream *s);

	struct rusage getRUsage( void );

	int getImageSize( void );

	int getDiskUsage( void );

	float bytesSent( void );
	float bytesReceived( void );

 private:

        /** After the schedd claims a resource, it puts it in a queue
            and then sends us a RESOURCE_AVAILABLE signal.  Upon
            receipt of that signal (it's registered in init()), we
            enter this function */
    int getResources( void );

        /** When we've got all the resources we need, we enter this
            function, which starts the mpi job */
    void startMaster();

        /** We will be given the args to start the mpi process by 
            the sneaky rsh program.  Start a Comrade with them */
    int startComrade(int cmd, Stream *s);

        /** Does necessary things to the args obtained from the 
            sneaky rsh. */
    void hackComradeAd( char *comradeArgs, ClassAd *ad );

        /** Pretty simple: takes the args, adds a -p4gp ..., puts
            the args back in. */
    void hackMasterAd( ClassAd *ad );
    
		/** A complex function that deals with the end of an MPI
			job.  It has two functions: 1) figure out if all the 
			resources should be told to kill themselves and 
			2) return TRUE if every resource is dead. 
		    @param exitReason The job exit reason.
		    @param exitStatus The exit status.*/
	int shutDownLogic( int& exitReason, int& exitStatus );

        /** The number of the next resource to start...when in start mode */
    int nextResourceToStart;

		/** The number of nodes for this job */
	int numNodes;

        /** TRUE if we're in the process of shutting down a job. */
    bool shutDownMode;

		/** Used to determine actual exit conditions...*/
	int actualExitReason;
	
		/** Used to determine actual exit conditions...*/
	int actualExitStatus;

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
	
    int printAdToFile(ClassAd *ad, char *JobHistoryFileName);

	int info_tid;	// DC id for our timer to get resource info 

};


#endif
