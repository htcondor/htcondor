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
	
		/** Makes the job go away.  Updates the job's classAd in the
			Q manager, sends email to the user if requested, and
			calls DC_Exit().  It doesn't return.
			@param reason The reason this job exited.
			@param exitStatus Status upon exit.
		 */
	void shutDown( int reason, int exitStatus );
	
		/**
		 */
	int handleJobRemoval(int sig);

	float bytesSent();
	float bytesReceived();

	int updateFromStarter(int command, Stream *s);

	struct rusage getRUsage( void );

	int getImageSize( void );

	int getDiskUsage( void );

 private:
	RemoteResource *remRes;

};



#endif




