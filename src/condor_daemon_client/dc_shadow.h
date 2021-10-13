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
	virtual bool locate( LocateType method=LOCATE_FULL );

		/** Send a ClassAd update with information about a job the
			shadow is running.
			@param ad ClassAd with info about the job
			@param insure_update Should we insure the update gets there?
			@return true on success, false on failure
		*/
	bool updateJobInfo( ClassAd* ad, bool insure_update = false );

		/** Get the password from the shadow that will be needed to run the job.

			This is obsolete, use getUserCredential (with a mode argument) instead.

			@param user The username under which to find the credential
			@param domain The domain under which to find the credential
			@param credential The result of fetching the credential
			@return true on success, false on failure
		*/
	bool getUserPassword( const char* user, const char* domain, std::string & passwd);

	/** Get the credential from the shadow that will be needed to run the job.

	@param user The username under which to find the credential
	@param domain The domain under which to find the credential
	@param type of credential to fetch. one of STORE_CRED_USER_KRB, STORE_CRED_USER_PWD or STORE_CRED_USER_OAUTH
	@return true on success, false on failure
	*/
	bool getUserCredential(const char *user, const char *domain, int mode, unsigned char* & cred, int & credlen);

 private:
	bool is_initialized;
	SafeSock* shadow_safesock;

		// I can't be copied (yet)
	DCShadow( const DCShadow& );
	DCShadow& operator = ( const DCShadow& );

};

#endif /* _CONDOR_DC_SHADOW_H */
