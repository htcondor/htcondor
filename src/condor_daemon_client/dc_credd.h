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


#ifndef __CONDOR_DC_CREDD_H__
#define __CONDOR_DC_CREDD_H__

#include "condor_common.h"
#include "condor_classad.h"
#include "condor_io.h"
#include "enum_utils.h"
#include "daemon.h"
#include "MyString.h"
#include "simplelist.h"

class Credential;


class DCCredd : public Daemon {
public:

		/** Constructor.  Same as a Daemon object.
		  @param name The name (or sinful string) of the daemon, NULL
		              if you want local  
		  @param pool The name of the pool, NULL if you want local
		*/
	DCCredd( const char* const name = NULL, const char* pool = NULL );

		/// Destructor
	~DCCredd();

	bool storeCredential (Credential * cred,
					CondorError & error);

	bool getCredentialData (const char * cred_name,
							void *& data,
							int & size,
							CondorError & error);

	bool listCredentials (SimpleList <Credential*> & result,  
						  int & size,
						  CondorError & error);

	bool removeCredential (const char * cred_name,
						   CondorError & error);
};
#endif /* _CONDOR_DC_CREDD_H */
