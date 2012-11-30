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


#ifndef _CONDOR_DC_TRANSFER_QUEUE_H
#define _CONDOR_DC_TRANSFER_QUEUE_H

/*
 DCTransferQueue is used to communicate with a transfer queue manager
 for getting permission to transfer a file (e.g. the shadow asking the
 schedd if it is ok to download an output file from a job).
*/

#include "condor_common.h"
#include "condor_classad.h"
#include "condor_io.h"
#include "enum_utils.h"
#include "daemon.h"
#include "MyString.h"

enum XFER_QUEUE_ENUM {
	XFER_QUEUE_NO_GO = 0,
	XFER_QUEUE_GO_AHEAD = 1
};

// TransferQueueContactInfo is used to pass around information for
// initializing DCTransferQueue.  For example, the schedd uses this to
// pass contact information to the shadow.  In addition to an address,
// this class contains information to optimize unlimited
// uploads/downloads, which do not require any connection to the
// manager to be made.

class TransferQueueContactInfo {
 public:
	TransferQueueContactInfo(char const *sinful,bool unlimited_uploads,bool unlimited_downloads);
		// this function takes the string representation produced by
		// GetStringRepresentation()
	TransferQueueContactInfo(char const *string_representation);
	TransferQueueContactInfo();
	void operator=(TransferQueueContactInfo const &copy);

		// serializes all contact information into a string suitable for
		// passing on a command line etc.
	bool GetStringRepresentation(std::string &str);

		// returns NULL if unlimited, o.w. sinful string of transfer
		// queue server
	char const *GetAddress() { return m_addr.c_str(); }

	bool GetUnlimitedUploads() { return m_unlimited_uploads; }
	bool GetUnlimitedDownloads() { return m_unlimited_downloads; }

 private:
	std::string m_addr;
	bool m_unlimited_uploads;
	bool m_unlimited_downloads;
};

class DCTransferQueue : public Daemon {
public:
	DCTransferQueue( TransferQueueContactInfo &contact_info );

		/// Destructor
	~DCTransferQueue();

		// Request permission to download (upload) file.
		// Follow up with one or more calls to PollForTransferQueueSlot()
		// to determine actual status.  This function just initiates
		// the request.  If transfers are unlimited, this does no work.
		// downloading - true if downloading file, false if uploading
	bool RequestTransferQueueSlot(bool downloading,char const *fname,char const *jobid,char const *queue_user,int timeout,MyString &error_desc);

		// See if we have been given permission to transfer currently
		// requested file.  If transfers are unlimited, this does no
		// work.  Call GoAheadAlways() to confirm whether we have GoAhead
		// for one file or for unlimited files.
	bool PollForTransferQueueSlot(int timeout,bool &pending,MyString &error_desc);

		// Remove current request for permission to transfer.
	void ReleaseTransferQueueSlot();

	bool GoAheadAlways( bool downloading );

 private:
		// As an optimization, if up/downloads are unlimited, this class
		// does not bother to connect to the transfer queue manager.
	bool m_unlimited_uploads;
	bool m_unlimited_downloads;

		// The current transfer request sock.  This stays open as long
		// as we are in the transfer queue.
	ReliSock *m_xfer_queue_sock;

	std::string m_xfer_fname;      // name of file involved in current request
	std::string m_xfer_jobid;      // job id associated with current request
	                            // (for information purposes only)

	bool m_xfer_downloading;    // true if request was for download
	bool m_xfer_queue_pending;  // true if status of request is pending
	bool m_xfer_queue_go_ahead; // true if it is ok to go ahead with transfer

	std::string m_xfer_rejected_reason;

		// Verify that transfer queue server hasn't revoked our slot.
	bool CheckTransferQueueSlot();
};

#endif
