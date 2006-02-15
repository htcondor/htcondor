/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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
	void init( ClassAd* job_ad, const char* schedd_addr );
	
		/** Shadow should spawn a new starter for this job.
		 */
	void spawn( void );

		/** Shadow should attempt to reconnect to a disconnected
			starter that might still be running for this job.  
		 */
	void reconnect( void );

	bool supportsReconnect( void );

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

	virtual void resourceReconnected( RemoteResource* rr );

	virtual void logDisconnectedEvent( const char* reason );

 protected:

	virtual void logReconnectedEvent( void );

	virtual void logReconnectFailedEvent( const char* reason );

 private:
	RemoteResource *remRes;
};



#endif




