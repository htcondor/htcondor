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

#ifndef _CLERK_H_
#define _CLERK_H_

#include "condor_classad.h"
#include "condor_commands.h"
//#include "totals.h"
#include "forkwork.h"
#include "clerk_stats.h"

#define MY_SUBSYSTEM "CLERK"

class CmDaemon : public Service
{
public:
	CmDaemon();
	~CmDaemon();

	// methods needed by dc_main
	void Init();   // called on startup
	void Config(); // called on startup and on reconfig
	void Shutdown(bool fast);

	// command handlers
	int receive_update(int, Stream*);
	int receive_update_expect_ack(int, Stream*);
	int receive_invalidation(int, Stream*);
	int receive_query(int, Stream*);

	// timer handlers
	int update_collector(); // send normal daemon updates to the collector.  called when ! ImTheCollector
	int send_collector_ad(); // send collector ad to upstream collectors, called when ImTheCollector

private:
	bool m_initialized;
	MyString m_daemonName;
	ClassAd m_daemonAd;
	CmStatistics stats;
	int m_idTimerSendUpdates;  // the id of the timer for sending updates upstream.

	// cached param values
	int m_UPDATE_INTERVAL;      // the rate at which we sent daemon ad updates upstream
	int m_CLIENT_TIMEOUT;
	int m_QUERY_TIMEOUT;
	int m_CLASSAD_LIFETIME;

	int FetchAds (AdTypes whichAds, ClassAd & query, List<ClassAd>& ads);
	int put_ad_v1(ClassAd *curr_ad, Stream* sock, ClassAd & query, bool v0);
	int put_ad_v2(ClassAd &ad, Stream* sock, ClassAd & query);
	int put_ad_v3(ClassAd &ad, Stream* sock, const classad::References * projection);

};

#ifndef NUMELMS
  #define NUMELMS(aa) (int)(sizeof(aa)/sizeof((aa)[0]))
#endif

#endif
