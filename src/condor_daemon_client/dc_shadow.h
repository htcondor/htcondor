/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department,
 *University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.
 *No use of the CONDOR Software Program Source Code is authorized
 *without the express consent of the CONDOR Team.  For more
 *information contact: CONDOR Team, Attention: Professor Miron Livny,
 *7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685,
 *(608) 262-0856 or miron@cs.wisc.edu.
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

#ifndef _CONDOR_DC_SHADOW_H
#define _CONDOR_DC_SHADOW_H

#include "condor_common.h"
#include "condor_classad.h"
#include "condor_io.h"


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
