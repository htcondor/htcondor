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

#ifndef __CONDOR_DC_CREDD_H__
#define __CONDOR_DC_CREDD_H__

#include "condor_common.h"
#include "condor_classad.h"
#include "condor_io.h"
#include "enum_utils.h"
#include "daemon.h"
#include "MyString.h"

class Credential;
template class SimpleList <Credential*>;


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
