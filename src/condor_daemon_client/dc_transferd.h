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
