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

#ifndef _CONDOR_DC_SHADOW_H
#define _CONDOR_DC_SHADOW_H

#include "condor_common.h"
#include "condor_classad.h"
#include "condor_io.h"


/** The subclass of the Daemon object for talking to a shadow
*/
class DCShadow : public Daemon {
public:

		/** Constructor.  The Shadow can't be found for the usual
			means, so you don't instantiate the DCShadow with the
			usual stuff (name, pool, etc).  You can't query the
			collector for it, or look for an address file, etc.  So,
			once you instantiate a DCShadow object, you have to call
			initFromClassAd() to initialize the DCShadow.
			@param name The name of the shadow, NULL if you don't know
		*/
	DCShadow( const char* name = NULL );

		/// Destructor.
	~DCShadow();

		/** Initialize this object with a ClassAd that contains the
			information we need.  At minimum, it must include
			ATTR_SHADOW_IP_ADDR.  Ideally, it'd also have
			ATTR_SHADOW_VERSION, ATTR_NAME, and ATTR_MACHINE.  
			@param ad ClassAd with info about the shadow.
			@return true if the ad was valid, false if not
		*/
	bool initFromClassAd( ClassAd* ad );

		/** The shadow can't be found through the usual means.
			Instead, when you instantiate a DCShadow object, you give
			it all the info in a ClassAd.  So, locate() just has to
			check that the ClassAd was valid and we have the info we
			need.  
			@return true if we have the info, false if not */
	bool locate( void );

		/** Send a ClassAd update with information about a job the
			shadow is running.
			@param ad ClassAd with info about the job
			@param insure_update Should we insure the update gets there?
			@return true on success, false on failure
		*/
	bool updateJobInfo( ClassAd* ad, bool insure_update = false );

 private:
	bool is_initialized;
	SafeSock* shadow_safesock;

		// I can't be copied (yet)
	DCShadow( const DCShadow& );
	DCShadow& operator = ( const DCShadow& );

};

#endif /* _CONDOR_DC_SHADOW_H */
