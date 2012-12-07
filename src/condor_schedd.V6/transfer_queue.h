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


#ifndef _CONDOR_TRANSFER_QUEUE_H
#define _CONDOR_TRANSFER_QUEUE_H


#include "dc_service.h"
#include "dc_transfer_queue.h"
#include "simplelist.h"
#include "reli_sock.h"

// transfer queue server's representation of a client
class TransferQueueRequest {
 public:
	TransferQueueRequest(ReliSock *sock,char const *fname,char const *jobid,char const *queue_user,bool downloading,time_t max_queue_age);
	~TransferQueueRequest();

	char const *Description();

	bool SendGoAhead(XFER_QUEUE_ENUM go_ahead=XFER_QUEUE_GO_AHEAD,char const *reason=NULL);

	ReliSock *m_sock;
	MyString m_queue_user;   // Name of file transfer queue user. (TRANSFER_QUEUE_USER_EXPR)
	MyString m_jobid;   // For information purposes, the job associated with
	                    // this file transfer.
	MyString m_fname;   // File this client originally requested to transfer.
	                    // In current implementation, it may silently move on
	                    // to a different file without notifying us.
	bool m_downloading; // true if client wants to download a file; o.w. upload
	bool m_gave_go_ahead; // true if we told this client to go ahead

	time_t m_max_queue_age; // clean transfer from queue after this time
	                        // 0 indicates no limit
	time_t m_time_born;
	time_t m_time_go_ahead;

	MyString m_description; // buffer for Description()
};

class TransferQueueManager: public Service {
 public:
	TransferQueueManager();
	~TransferQueueManager();

	void InitAndReconfig();

	void RegisterHandlers();

	int HandleRequest(int cmd,Stream *stream);

	int HandleDisconnect( Stream *sock );

		// Iterate through queue, pruning disconnected clients, and
		// giving the go ahead to those that deserve it.
	void CheckTransferQueue();

		// This is called to register a future call to CheckTransferQueue.
	void TransferQueueChanged();

	bool GetContactInfo(char const *command_sock_addr, std::string &contact_str);

	int GetNumUploading() { return m_uploading; }
	int GetNumDownloading() { return m_downloading; }
	int GetNumWaitingToUpload() { return m_waiting_to_upload; }
	int GetNumWaitingToDownload() { return m_waiting_to_download; }
	int GetUploadWaitTime() { return m_upload_wait_time; }
	int GetDownloadWaitTime() { return m_download_wait_time; }
	int GetMaxUploading() { return m_max_uploads; }
	int GetMaxDownloading() { return m_max_downloads; }
 private:
	SimpleList<TransferQueueRequest *> m_xfer_queue;
	int m_max_uploads;   // 0 if unlimited
	int m_max_downloads; // 0 if unlimited
	time_t m_default_max_queue_age; // 0 if unlimited

	int m_check_queue_timer;

	int m_uploading;
	int m_downloading;
	int m_waiting_to_upload;
	int m_waiting_to_download;
	int m_upload_wait_time;
	int m_download_wait_time;

	unsigned int m_round_robin_counter; // increments each time we send GoAhead to a client
	std::map< std::string,unsigned int > m_round_robin_recency; // key = queue_user, value = round robin counter at time of last GoAhead

	unsigned int m_round_robin_garbage_counter;
	time_t m_round_robin_garbage_time;

	bool AddRequest( TransferQueueRequest *client );
	void RemoveRequest( TransferQueueRequest *client );
	int GetRoundRobinRecency(const std::string &queue);
	void SetRoundRobinRecency(const std::string &queue);
	void CollectRoundRobinGarbage();
};


#endif
