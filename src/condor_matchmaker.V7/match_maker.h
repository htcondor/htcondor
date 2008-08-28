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

#ifndef __MATCH_MAKER_H__
#define __MATCH_MAKER_H__

using namespace std;

#include <list>
#include <map>

#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "dc_collector.h"

#include "match_maker_resources.h"

#ifndef WANT_CLASSAD_NAMESPACE
# define WANT_CLASSAD_NAMESPACE
#endif
#include "classad/classad_distribution.h"


class MatchMakerIntervalTimer;

class MatchMaker : public Service
{
	friend class MatchMakerIntervalTimer;

  public:
	// ctor/dtor
	MatchMaker( void );
	~MatchMaker( void );
	int init( const char *name = NULL );
	int config( void );
	int shutdownFast(void);
	int shutdownGraceful(void);

  protected:
	bool ParamInt( const char *param_name, int &value,
				   int default_value,
				   int min_value = INT_MIN, int max_value = INT_MAX );

  private:
	// Command handlers
	int commandHandler_GetMatch(int command, Stream *stream);
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
		list<const MatchMakerLease *>	&lease_list
		);
	bool GetLeases(
		Stream							*stream,
		list<MatchMakerLease *>			&lease_list
		);

	CollectorList*				m_collectorList;
	MatchMakerResources			m_resources;
	ClassAd						m_publicAd;
	char						*m_myName;

	// Details on how to query the collector
	AdTypes						m_queryAdtypeNum;
	string						m_queryAdtypeStr;
	string						m_queryConstraints;

	MatchMakerIntervalTimer		*m_GetAdsTimer;
	MatchMakerIntervalTimer		*m_UpdateTimer;
	MatchMakerIntervalTimer		*m_PruneTimer;
	int							m_TimerId_GetAds;
	int							m_Interval_GetAds;
	int							m_TimerId_Update;
	int							m_Interval_Update;
	int							m_TimerId_Prune;
	int							m_Interval_Prune;
};



#endif//__MATCH_MAKER_H__
