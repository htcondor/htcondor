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

#ifndef _CONDOR_DC_STARTER_H
#define _CONDOR_DC_STARTER_H

#include "condor_common.h"
#include "condor_classad.h"
#include "condor_io.h"


/** The subclass of the Daemon object for talking to a starter
*/
class DCStarter : public Daemon {
public:

		/** Constructor.  The Starter can't be found for the usual
			means, so you don't instantiate the DCStarter with the
			usual stuff (name, pool, etc).  You can't query the
			collector for it, or look for an address file, etc.  So,
			once you instantiate a DCStarter object, you have to call
			initFromClassAd() to initialize the DCStarter.
			@param name The name of the starter, NULL if you don't know
		*/
	DCStarter( const char* name = NULL );

		/// Destructor.
	~DCStarter();

		/** Initialize this object with a ClassAd that contains the
			information we need.  At minimum, it must include
			ATTR_STARTER_IP_ADDR.  Ideally, it'd also have
			ATTR_VERSION, ATTR_NAME, and ATTR_MACHINE.  
			@param ad ClassAd with info about the starter.
			@return true if the ad was valid, false if not
		*/
	bool initFromClassAd( ClassAd* ad );

		/** The starter can't be found through the usual means.
			Instead, when you instantiate a DCStarter object, you give
			it all the info in a ClassAd.  So, locate() just has to
			check that the ClassAd was valid and we have the info we
			need.  
			@return true if we have the info, false if not */
	bool locate( void );

	bool reconnect( ClassAd* req, ClassAd* reply, ReliSock* rsock, 
					int timeout = -1 );

	// Error - Failed to update, a problem
	// Okay - Success.  Updated
	// Declined - Success.  Other side doesn't want it.  Don't bother
	//      to try again.
	enum X509UpdateStatus { XUS_Error = 0, XUS_Okay = 1, XUS_Declined = 2 };
	X509UpdateStatus updateX509Proxy(const char * filename);

 private:
	bool is_initialized;

		// I can't be copied (yet)
	DCStarter( const DCStarter& );
	DCStarter& operator = ( const DCStarter& );

};

#endif /* _CONDOR_DC_STARTER_H */
