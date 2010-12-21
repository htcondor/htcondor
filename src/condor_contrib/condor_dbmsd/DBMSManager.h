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

#ifndef _DBMSMANAGER_H_
#define _DBMSMANAGER_H_

#include "condor_common.h"
#include "condor_daemon_core.h"

class DBMSManager: public Service {
public:
	DBMSManager();
	virtual ~DBMSManager();
	void init();
	void config();
	void stop();

	char const *Name() const {return m_name.Value();}

private:
	MyString m_name;

	ClassAd m_public_ad;
	int m_public_ad_update_interval;
	int m_public_ad_update_timer;
	int m_database_purge_interval;
	int m_database_purge_timer;
	int m_database_reindex_interval;
	int m_database_reindex_timer;

	class CollectorList *m_collectors;
	class ManagedDatabase **m_databases;

	void InitPublicAd();
	void TimerHandler_UpdateCollector();
	void InvalidatePublicAd();

	void TimerHandler_PurgeDatabase();
	void TimerHandler_ReindexDatabase();
};

#endif
