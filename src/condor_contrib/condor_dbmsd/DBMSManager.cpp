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


#include "condor_common.h"
#include "DBMSManager.h"
#include "condor_config.h"
#include "dc_collector.h"
#include "get_daemon_name.h"
#include "ManagedDatabase.h"

DBMSManager::DBMSManager() {
	m_collectors = NULL;
	m_public_ad_update_timer = -1;
	m_public_ad_update_interval = -1;
	m_databases = NULL;
	m_database_purge_interval = -1;
	m_database_purge_timer = -1;
	m_database_reindex_interval = -1;
	m_database_reindex_timer = -1;
}

DBMSManager::~DBMSManager() {
	if(m_collectors) {
		delete m_collectors;
	}
	if(m_databases) {
		int i = 0;
		while (m_databases[i]) {
			delete m_databases[i];
			m_databases[i] = NULL;
			i++;
		}
		delete[] m_databases;
	}
}

void
DBMSManager::init() {
	m_collectors = CollectorList::create();

		// create ManagedDatabase object
	m_databases = (ManagedDatabase **) malloc(2*sizeof (ManagedDatabase *));
	
		/* the default database will use what's specified in the config */
	m_databases[0] = new ManagedDatabase();
		/* end the list with a null */
	m_databases[1] = (ManagedDatabase *)0;

	config();
}

void
DBMSManager::stop() {
	// this should probably disconnect from any open databases
	InvalidatePublicAd();
}

void
DBMSManager::InitPublicAd() {
	m_public_ad = ClassAd();

	//TODO: this is the wrong ADTYPE.
	//Use the new generic ad type when it becomes available.
	//Until then, actual writes to the collector are disabled,
	//because it conflicts with the schedd on the same host..

	SetMyTypeName( m_public_ad, DBMSD_ADTYPE );
	SetTargetTypeName( m_public_ad, "" );

	m_public_ad.Assign(ATTR_MACHINE,my_full_hostname());
	m_public_ad.Assign(ATTR_NAME,m_name.Value());

	config_fill_ad( &m_public_ad );
}

void
DBMSManager::config() {
	char *name = param("DBMSMANAGER_NAME");
	if(name) {
		char *valid_name = build_valid_daemon_name(name);
		m_name = valid_name;
		free(name);
		delete [] valid_name;
	}
	else {
		char *default_name = default_daemon_name();
		if(default_name) {
			m_name = default_name;
			delete [] default_name;
		}
	}

	InitPublicAd();

	int update_interval = param_integer("UPDATE_INTERVAL", 60);
	if(m_public_ad_update_interval != update_interval) {
		m_public_ad_update_interval = update_interval;

		if(m_public_ad_update_timer >= 0) {
			daemonCore->Cancel_Timer(m_public_ad_update_timer);
			m_public_ad_update_timer = -1;
		}
		dprintf(D_FULLDEBUG, "Setting update interval to %d\n",
				m_public_ad_update_interval);
		m_public_ad_update_timer = daemonCore->Register_Timer(
			0,
			m_public_ad_update_interval,
			(TimerHandlercpp)&DBMSManager::TimerHandler_UpdateCollector,
			"DBMSManager::TimerHandler_UpdateCollector",
			this);
	}

		/* register the database purging callback */
	int purge_interval = param_integer("DATABASE_PURGE_INTERVAL", 86400); // 24 hours

	if(m_database_purge_interval != purge_interval) {
		m_database_purge_interval = purge_interval;

		if(m_database_purge_timer >= 0) {
			daemonCore->Cancel_Timer(m_database_purge_timer);
			m_database_purge_timer = -1;
		}
		dprintf(D_FULLDEBUG, "Setting database purge interval to %d\n",
				m_database_purge_interval);
		m_database_purge_timer = daemonCore->Register_Timer(
			0,
			m_database_purge_interval,
			(TimerHandlercpp)&DBMSManager::TimerHandler_PurgeDatabase,
			"DBMSManager::TimerHandler_PurgeDatabase",
			this);
	}

		/* register the database reindexing callback */
	int reindex_interval = param_integer("DATABASE_REINDEX_INTERVAL", 86400); // 24 hours

	if(m_database_reindex_interval != reindex_interval) {
		m_database_reindex_interval = reindex_interval;

		if(m_database_reindex_timer >= 0) {
			daemonCore->Cancel_Timer(m_database_reindex_timer);
			m_database_reindex_timer = -1;
		}
		dprintf(D_FULLDEBUG, "Setting database reindex interval to %d\n",
				m_database_reindex_interval);
		m_database_reindex_timer = daemonCore->Register_Timer(
			0,
			m_database_reindex_interval,
			(TimerHandlercpp)&DBMSManager::TimerHandler_ReindexDatabase,
			"DBMSManager::TimerHandler_ReindexDatabase",
			this);
	}
	
}

void
DBMSManager::TimerHandler_UpdateCollector() {
    ASSERT(m_collectors != NULL);
	m_collectors->sendUpdates(UPDATE_AD_GENERIC, &m_public_ad, NULL, true);
	return;
}

void
DBMSManager::InvalidatePublicAd() {
	ClassAd query_ad;
    SetMyTypeName(query_ad, QUERY_ADTYPE);
    SetTargetTypeName(query_ad, DBMSD_ADTYPE);

    MyString line;
    line.formatstr("%s = TARGET.%s == \"%s\"", ATTR_REQUIREMENTS, ATTR_NAME, m_name.Value());
    query_ad.Insert(line.Value());
	query_ad.Assign(ATTR_NAME,m_name.Value());

    m_collectors->sendUpdates(INVALIDATE_ADS_GENERIC, &query_ad, NULL, true);
	
	//TODO/FIXME - delete the ads of the databases we're advertising

	return;
}

void
DBMSManager::TimerHandler_PurgeDatabase() {
	dprintf(D_ALWAYS, "******** Start of Purging Old Data ********\n");
	if(m_databases) {
		int i = 0;
		while (m_databases[i]) {
			m_databases[i]->PurgeDatabase();
			i++;
		}
	}	
	dprintf(D_ALWAYS, "******** End of Purging Old Data ********\n");
}

void
DBMSManager::TimerHandler_ReindexDatabase() {
	dprintf(D_ALWAYS, "******** Start of Reindexing Database ********\n");
	if(m_databases) {
		int i = 0;
		while (m_databases[i]) {
			m_databases[i]->ReindexDatabase();
			i++;
		}
	}	
	dprintf(D_ALWAYS, "******** End of Reindexing Database ********\n");
}
