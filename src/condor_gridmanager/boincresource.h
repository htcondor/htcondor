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


#ifndef BOINCRESOURCE_H
#define BOINCRESOURCE_H

#include "condor_common.h"
#include "condor_daemon_core.h"
#include <map>

#include "baseresource.h"
#include "gahp-client.h"


class BoincJob;

struct BoincBatch;

enum BoincSubmitResponse {
	BoincSubmitWait,
	BoincSubmitSuccess,
	BoincSubmitFailure
};

class BoincResource : public BaseResource
{
 protected:
	BoincResource( const char *resource_name, const char *authenticator );
	~BoincResource();

 public:
	bool Init();
	const char *ResourceType();
	void Reconfig();
	void RegisterJob( BaseJob *job );
	void UnregisterJob( BaseJob *job );

	const char *GetHashName();

	void PublishResourceAd( ClassAd *resource_ad );

	bool JoinBatch( BoincJob *job, std::string &batch_name,
					std::string &error_str );
	BoincSubmitResponse Submit( BoincJob *job, std::string &error_str );
	bool JobDone( BoincJob *job );

	static std::string & HashName( const char *resource_name,
								 const char *authenticator );

	static BoincResource *FindOrCreateResource( const char *resource_name,
												const char *authenticator );

	static void setGahpCallTimeout( int new_timeout )
		{ gahpCallTimeout = new_timeout; }

	static bool BatchReadyToSubmit( BoincBatch *batch, unsigned *delay = NULL );

	// This should be private, but BoincJob references it directly for now
	static std::map <std::string, BoincResource *> ResourcesByName;

	char *m_serviceUri;
	char *m_authenticator;

 private:
	void DoPing( unsigned& ping_delay, bool& ping_complete,
				 bool& ping_succeeded );

	void UpdateBoincLeases();

	bool initialized;

	void DoBatchSubmits();

	static int gahpCallTimeout;
	GahpClient *gahp; // For pings.
	GahpClient *m_statusGahp;
	GahpClient *m_leaseGahp;
	GahpClient *m_submitGahp;

	// TODO add a ready-to-submit set?
	//   This could avoid copious rechecks for whether a batch is ready
	//   to be submitted.
	//   Could instead add new BatchSubmitStatus BatchWaitingToSubmit
	std::list<BoincBatch *> m_batches;
	StringList m_statusBatches;
	int m_leaseTid;
	BoincBatch *m_activeLeaseBatch;
	int m_activeLeaseTime;
	int m_submitTid;
	BoincBatch *m_activeSubmitBatch;

	// A full query is one that obtains the status of all jobs in a
	// subset of batches (those batches that have m_need_full_query==true).
	// A partial query is one that returns jobs whose status has changed
	// since the last partial status, for all batches. The first partial
	// query asks for changes since time 0 (essential a full query for
	// all batches).
	bool m_needFullQuery;
	bool m_doingFullQuery;
	std::string m_lastQueryTime;

protected:

	BatchStatusResult StartBatchStatus();
	BatchStatusResult FinishBatchStatus();
	GahpClient * BatchGahp();

//	void DoUpdateSharedLease( unsigned& update_delay, bool& update_complete, 
//							  bool& update_succeeded );
};

#endif
