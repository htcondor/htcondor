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

#ifndef TRANSFERREQUEST_H
#define TRANSFERREQUEST_H

#include "condor_common.h"
#include "proxymanager.h"
#include "gahp-client.h"

#define TRANSFER_QUEUED		1
#define TRANSFER_ACTIVE		2
#define TRANSFER_DONE		3
#define TRANSFER_FAILED		4

class TransferRequest : public Service
{
 public:
	TransferRequest( Proxy *proxy, const StringList &src_list,
					 const StringList &dst_list, int notify_tid );
	virtual ~TransferRequest();

	enum TransferStatus {
		TransferQueued,
		TransferActive,
		TransferDone,
		TransferFailed,
	};

	//static info on active transfers

	void CheckRequest();
	int m_CheckRequest_tid;

	StringList m_src_urls;
	StringList m_dst_urls;
	Proxy *m_proxy;
	int m_notify_tid;
	GahpClient *m_gahp;

	TransferStatus m_status;
	std::string m_errMsg;
};

#endif
