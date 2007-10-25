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

#ifndef _CONDOR_TRANSFER_QUEUE_H
#define _CONDOR_TRANSFER_QUEUE_H


#include "dc_service.h"
#include "dc_transfer_queue.h"
#include "simplelist.h"
#include "reli_sock.h"

// transfer queue server's representation of a client
class TransferQueueRequest {
 public:
	TransferQueueRequest(ReliSock *sock,char const *fname,char const *jobid,bool downloading,time_t max_queue_age);
	~TransferQueueRequest();

	char const *Description();

	bool Alive();

	bool SendGoAhead(XFER_QUEUE_ENUM go_ahead=XFER_QUEUE_GO_AHEAD,char const *reason=NULL);

	ReliSock *m_sock;
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

	void GetContactInfo(char const *command_sock_addr,MyString &contact_str);
 private:
	SimpleList<TransferQueueRequest *> m_xfer_queue;
	int m_max_uploads;   // 0 if unlimited
	int m_max_downloads; // 0 if unlimited
	time_t m_default_max_queue_age; // 0 if unlimited

	int m_check_queue_timer;

	bool AddRequest( TransferQueueRequest *client );
	void RemoveRequest( TransferQueueRequest *client );
};


#endif
