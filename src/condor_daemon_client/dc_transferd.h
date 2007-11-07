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


#ifndef _CONDOR_DC_TRANSFERD_H
#define _CONDOR_DC_TRANSFERD_H

#include "condor_common.h"
#include "condor_classad.h"
#include "condor_io.h"

/** The subclass of the Daemon object for talking to a transferd
*/
class DCTransferD : public Daemon {
public:

		/** Constructor.
		*/
	DCTransferD( const char* name = NULL, const char* pool = NULL );

		/// Destructor.
	~DCTransferD();

	// contact the transferd to establish a transfer request channel
	// returns true if such a channel could be set up
	bool setup_treq_channel(ReliSock **td_req_sock, int timeout,
		CondorError *errstack);
	
	// connect to the transferd and upload the files with the FileTransfer
	// object
	bool upload_job_files(int JobAdsArrayLen, ClassAd* JobAdsArray[], 
		ClassAd *work_ad, CondorError * errstack);

	// connect to the transferd and download the files specified in the
	// work_ad.
	bool download_job_files(ClassAd *work_ad, CondorError * errstack);

 private:

		// I can't be copied (yet)
	DCTransferD( const DCTransferD& );
	DCTransferD& operator = ( const DCTransferD& );

};

#endif /* _CONDOR_DC_TRANSFERD_H */
