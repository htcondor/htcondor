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

#ifndef SHADOW_H
#define SHADOW_H

#include "condor_common.h"
#include "baseshadow.h"
#include "remoteresource.h"

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
	void init( ClassAd *jobAd, char schedd_addr[], char host[], 
			   char capability[], char cluster[], char proc[]);
	
		/**
		 */
	int handleJobRemoval(int sig);

		/// Log an execute event to the UserLog
	void logExecuteEvent( void );

		/** Return the exit reason for this shadow's job.  Since we've
			only got 1 RemoteResource, we can just return its value.
		*/
	int getExitReason( void );

	float bytesSent();
	float bytesReceived();

	int updateFromStarter(int command, Stream *s);

	struct rusage getRUsage( void );

	int getImageSize( void );

	int getDiskUsage( void );

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
	virtual void emailTerminateEvent( int exitReason );

		/** Do all work to cleanup before this shadow can exit.  We've
			only got 1 RemoteResource to kill the starter on.
		*/
	virtual void cleanUp( void );

		/** Do a graceful shutdown of the remote starter */
	virtual void gracefulShutDown( void );

	virtual void resourceBeganExecution( RemoteResource* rr );

 private:
	RemoteResource *remRes;
};



#endif




