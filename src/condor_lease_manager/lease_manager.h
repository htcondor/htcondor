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

#ifndef __LEASE_MANAGER_H__
#define __LEASE_MANAGER_H__

using namespace std;

#include <list>
#include <map>

#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "dc_collector.h"

#include "lease_manager_resources.h"

#ifndef WANT_CLASSAD_NAMESPACE
# define WANT_CLASSAD_NAMESPACE
#endif
#include "classad/classad_distribution.h"


class LeaseManagerIntervalTimer;

class LeaseManager : public Service
{
	friend class LeaseManagerIntervalTimer;

  public:
	// ctor/dtor
	LeaseManager( void );
	~LeaseManager( void );
	int init( void );
	int config( void );
	int shutdownFast(void);
	int shutdownGraceful(void);

  protected:
	bool ParamInt( const char *param_name, int &value,
				   int default_value,
				   int min_value = INT_MIN, int max_value = INT_MAX );

  private:
	// Command handlers
	int commandHandler_GetLeases(int command, Stream *stream);
	int commandHandler_RenewLease(int command, Stream *stream);
	int commandHandler_ReleaseLease(int command, Stream *stream);

	// Timer handlers
	int timerHandler_GetAds( void );
	int timerHandler_Update( void );
	int timerHandler_Prune( void );

	// Initialize the public ad
	int initPublicAd( void );

	// Send an ad list to a stream
	bool SendLeases(
		Stream							*stream,
		list<const LeaseManagerLease *>	&lease_list
		);
	bool GetLeases(
		Stream							*stream,
		list<LeaseManagerLease *>		&lease_list
		);

	CollectorList*				 m_collectorList;
	LeaseManagerResources		 m_resources;
	ClassAd						 m_publicAd;

	// Details on how to query the collector
	AdTypes						 m_queryAdtypeNum;
	string						 m_queryAdtypeStr;
	string						 m_queryConstraints;

	LeaseManagerIntervalTimer	*m_GetAdsTimer;
	LeaseManagerIntervalTimer	*m_UpdateTimer;
	LeaseManagerIntervalTimer	*m_PruneTimer;
	int							 m_TimerId_GetAds;
	int							 m_Interval_GetAds;
	int							 m_TimerId_Update;
	int							 m_Interval_Update;
	int							 m_TimerId_Prune;
	int							 m_Interval_Prune;

	bool						 m_enable_ad_debug;
};



#endif//__LEASE_MANAGER_H__
