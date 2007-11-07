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


#ifndef _CONDOR_DC_MASTER_H
#define _CONDOR_DC_MASTER_H

#include "condor_common.h"
#include "condor_classad.h"
#include "condor_io.h"


/** The subclass of the Daemon object for talking to a shadow
*/
class DCMaster : public Daemon {
public:

	DCMaster( const char* name = NULL );

		/// Destructor.
	~DCMaster();

	bool sendMasterOff( bool insure_update = false  );

 private:
	bool m_is_initialized;
	SafeSock* m_master_safesock;

	// I can't be copied (yet)
	DCMaster( const DCMaster& );
	DCMaster& operator = ( const DCMaster& );
	bool sendMasterCommand( bool insure_update, int my_cmd );

};

#endif /* _CONDOR_DC_MASTER_H */
