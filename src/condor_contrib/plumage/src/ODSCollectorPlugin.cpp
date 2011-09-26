/*
 * Copyright 2009-2011 Red Hat, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include "condor_common.h"

// local includes
#include "ODSMongodbOps.h"
#include "ODSPoolUtils.h"

// seems boost meddles with assert defs
#include "assert.h"
#include "condor_config.h"
#include "../condor_collector.V6/CollectorPlugin.h"
#include "hashkey.h"
#include "../condor_collector.V6/collector.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

using namespace std;
using namespace mongo;
using namespace plumage::etl;

int historyInterval;
int initialDelay;
int historyTimer;

const char DB_RAW_ADS[] = "condor_raw.ads";
const char DB_STATS_SAMPLES[] = "condor_stats.samples";
const char DB_STATS_SAMPLES_SUB[] = "condor_stats.samples.submitter";
const char DB_STATS_SAMPLES_MACH[] = "condor_stats.samples.machine";

class ODSCollectorPlugin : public Service, CollectorPlugin
{
	string m_name;
	string m_ip;
	ODSMongodbOps* m_ads_conn;
	ODSMongodbOps* m_stats_conn;

    void
    processSubmitterStats() {
        dprintf(D_FULLDEBUG, "ODSCollectorPlugin::processSubmitterStats called...\n");
        mongo::DBClientConnection* conn =  m_ads_conn->m_db_conn;
        auto_ptr<DBClientCursor> cursor = conn->query(DB_RAW_ADS, QUERY( ATTR_MY_TYPE << "Submitter" ) );
        while( cursor->more() ) {
            BSONObj p = cursor->next();
            dprintf(D_FULLDEBUG, "Submitter %s:\t%s=%d\t%s=%d\t%s=%d\n",
                    p.getStringField(ATTR_NAME),
                    ATTR_RUNNING_JOBS,p.getIntField(ATTR_RUNNING_JOBS),
                    ATTR_HELD_JOBS,p.getIntField(ATTR_HELD_JOBS),
                    ATTR_IDLE_JOBS,p.getIntField(ATTR_IDLE_JOBS));

            // write record to submitter samples
            BSONObjBuilder bob;
            bob << "ts" << mongo::DATENOW;
            bob.append("sn",p.getStringField(ATTR_NAME));
            bob.append("mn",p.getStringField(ATTR_MACHINE));
            bob.append("jr",p.getIntField(ATTR_RUNNING_JOBS));
            bob.append("jh",p.getIntField(ATTR_HELD_JOBS));
            bob.append("ji",p.getIntField(ATTR_IDLE_JOBS));
            conn->insert(DB_STATS_SAMPLES_SUB,bob.obj());
        }
    }

    void
    processMachineStats() {
        dprintf(D_FULLDEBUG, "ODSCollectorPlugin::processMachineStats() called...\n");
        mongo::DBClientConnection* conn =  m_ads_conn->m_db_conn;
        auto_ptr<DBClientCursor> cursor = conn->query(DB_RAW_ADS, QUERY( ATTR_MY_TYPE << "Machine" ) );
        while( cursor->more() ) {
            BSONObj p = cursor->next();
            dprintf(D_FULLDEBUG, "Machine %s:\t%s=%s\t%s=%s\t%s=%d\t%s=%f\t%s=%s\n",
                    p.getStringField(ATTR_NAME),
                    ATTR_ARCH,p.getStringField(ATTR_ARCH),
                    ATTR_OPSYS,p.getStringField(ATTR_OPSYS),
                    ATTR_KEYBOARD_IDLE,p.getIntField(ATTR_KEYBOARD_IDLE),
                    ATTR_LOAD_AVG,p.getField(ATTR_LOAD_AVG).Double(),
                    ATTR_STATE,p.getStringField(ATTR_STATE));

            // write record to machine samples
            BSONObjBuilder bob;
            bob << "ts" << mongo::DATENOW;
            bob.append("mn",p.getStringField(ATTR_NAME));
            bob.append("ar",p.getStringField(ATTR_ARCH));
            bob.append("os",p.getStringField(ATTR_OPSYS));
            bob.append("ki",p.getIntField(ATTR_KEYBOARD_IDLE));
            bob.append("la",p.getField(ATTR_LOAD_AVG).Double());
            bob.append("st",p.getStringField(ATTR_STATE));
            conn->insert(DB_STATS_SAMPLES_MACH,bob.obj());
        }
    }

    void
    processStatsTimer() {
        dprintf(D_FULLDEBUG, "ODSCollectorPlugin::processStatsTimer() called\n");
        processSubmitterStats();
        processMachineStats();
    }

public:
    ODSCollectorPlugin(): m_ads_conn(NULL), m_stats_conn(NULL)
    {
        //
    }

	void
	initialize()
	{
		stringstream dbhost;
		int dbport;

		dprintf(D_FULLDEBUG, "ODSCollectorPlugin: Initializing...\n");

		m_name = getPoolName();
		m_ip = my_ip_string();
		
		char* tmp = NULL;
		if (NULL != (tmp = param("ODS_DB_HOST"))) {
			dbhost << tmp;
			free (tmp);
		}
		else {
			dbhost << "localhost";
		}

		if (param_integer("ODS_DB_PORT",dbport,false,0)) {
			dbhost << ":" << dbport;
		}

        m_ads_conn = new ODSMongodbOps(DB_RAW_ADS);
        if (!m_ads_conn->init(dbhost.str())) {
			EXCEPT("Failed to initialize DB connection");
		}

        m_stats_conn = new ODSMongodbOps(DB_STATS_SAMPLES);
        if (!m_stats_conn->init(dbhost.str())) {
			EXCEPT("Failed to initialize DB connection");
		}

        historyInterval = param_integer("POOL_HISTORY_SAMPLING_INTERVAL",20);
        initialDelay=param_integer("UPDATE_INTERVAL",60);

        // Register timer for writing stats to DB
        if (-1 == (historyTimer =
            daemonCore->Register_Timer(
                0,
                20,
                (TimerHandlercpp)(&ODSCollectorPlugin::processStatsTimer),
                "Timer for collecting ODS stats",
                this
            ))) {
            EXCEPT("Failed to register ODS stats timer");
            }
	}

	void
	shutdown()
	{

		dprintf(D_FULLDEBUG, "ODSCollectorPlugin: shutting down...\n");
		delete m_ads_conn;
        delete m_stats_conn;

	}

	void
	update(int command, const ClassAd &ad)
	{
		MyString name,machine;
		AdNameHashKey hashKey;

		// TODO: const hack for make*HashKey and dPrint
		ClassAd* _ad = const_cast<ClassAd *> (&ad);

        // TODO: ret check this...
        _ad->LookupString(ATTR_NAME,name);

		BSONObjBuilder key;
		key.append(ATTR_NAME,name);

		switch (command) {
		case UPDATE_STARTD_AD:
			dprintf(D_FULLDEBUG, "ODSCollectorPlugin: Received UPDATE_STARTD_AD\n");
			if (param_boolean("ODS_IGNORE_UPDATE_STARTD_AD", FALSE)) {
				dprintf(D_FULLDEBUG, "ODSCollectorPlugin: Configured to ignore UPDATE_STARTD_AD\n");
				break;
			}

			if (!makeStartdAdHashKey(hashKey, _ad, NULL)) {
				dprintf(D_FULLDEBUG, "Could not make hashkey -- ignoring ad\n");
			}

			m_ads_conn->updateAd(key,_ad);

			break;
        case UPDATE_SUBMITTOR_AD:
            dprintf(D_FULLDEBUG, "ODSCollectorPlugin: Received UPDATE_SUBMITTOR_AD\n");
            if (param_boolean("ODS_IGNORE_UPDATE_SUBMITTOR_AD", FALSE)) {
                dprintf(D_FULLDEBUG, "ODSCollectorPlugin: Configured to ignore UPDATE_SUBMITTOR_AD\n");
                break;
            }

            if (!makeGenericAdHashKey(hashKey, _ad, NULL)) {
                dprintf(D_FULLDEBUG, "Could not make hashkey -- ignoring ad\n");
            }

            // TODO: ret check this...
            _ad->LookupString(ATTR_MACHINE,machine);
            key.append(ATTR_MACHINE, machine);

            m_ads_conn->updateAd(key,_ad);

            break;
		case UPDATE_NEGOTIATOR_AD:
			dprintf(D_FULLDEBUG, "ODSCollectorPlugin: Received UPDATE_NEGOTIATOR_AD\n");
			if (param_boolean("ODS_IGNORE_UPDATE_NEGOTIATOR_AD", TRUE)) {
				dprintf(D_FULLDEBUG, "ODSCollectorPlugin: Configured to ignore UPDATE_NEGOTIATOR_AD\n");
				break;
			}

			if (!makeNegotiatorAdHashKey(hashKey, _ad, NULL)) {
				dprintf(D_FULLDEBUG, "Could not make hashkey -- ignoring ad\n");
			}

            m_ads_conn->updateAd(key,_ad);

			break;
		case UPDATE_SCHEDD_AD:
			dprintf(D_FULLDEBUG, "ODSCollectorPlugin: Received UPDATE_SCHEDD_AD\n");
			if (param_boolean("ODS_IGNORE_UPDATE_SCHEDD_AD", TRUE)) {
				dprintf(D_FULLDEBUG, "ODSCollectorPlugin: Configured to ignore UPDATE_SCHEDD_AD\n");
				break;
			}

			if (!makeScheddAdHashKey(hashKey, _ad, NULL)) {
				dprintf(D_FULLDEBUG, "Could not make hashkey -- ignoring ad\n");
			}

            m_ads_conn->updateAd(key,_ad);

			break;
		case UPDATE_GRID_AD:
			dprintf(D_FULLDEBUG, "ODSCollectorPlugin: Received UPDATE_GRID_AD\n");
            if (param_boolean("ODS_IGNORE_UPDATE_GRID_AD", TRUE)) {
                dprintf(D_FULLDEBUG, "ODSCollectorPlugin: Configured to ignore UPDATE_GRID_AD\n");
                break;
            }

			if (!makeGridAdHashKey(hashKey, _ad, NULL)) {
				dprintf(D_FULLDEBUG, "Could not make hashkey -- ignoring ad\n");
			}

            m_ads_conn->updateAd(key,_ad);

			break;
		case UPDATE_COLLECTOR_AD:
            dprintf(D_FULLDEBUG, "ODSCollectorPlugin: Received UPDATE_COLLECTOR_AD\n");
            if (param_boolean("ODS_IGNORE_UPDATE_COLLECTOR_AD", TRUE)) {
                dprintf(D_FULLDEBUG, "ODSCollectorPlugin: Configured to ignore UPDATE_COLLECTOR_AD\n");
                break;
            }
				// We could receive collector ads from many
				// collectors, but we only maintain our own. So,
				// ignore all others.
			char *str;
			if (ad.LookupString(ATTR_MY_ADDRESS, &str)) {
				string public_addr(str);
				free(str);

				if (public_addr != m_ip) {
                    m_ads_conn->updateAd(key,_ad);
				}
			}
			break;
		default:
			dprintf(D_FULLDEBUG, "ODSCollectorPlugin: Unsupported command: %s\n",
					getCollectorCommandString(command));
		}
	}

	void
	invalidate(int command, const ClassAd &ad)
	{
		AdNameHashKey hashKey;

		// TODO: const hack for make*HashKey
		ClassAd* _ad = const_cast<ClassAd *> (&ad);

		switch (command) {
			case INVALIDATE_STARTD_ADS:
				dprintf(D_FULLDEBUG, "ODSCollectorPlugin: Received INVALIDATE_STARTD_ADS\n");
				if (!makeStartdAdHashKey(hashKey, _ad, NULL)) {
					dprintf(D_FULLDEBUG, "Could not make hashkey -- ignoring ad\n");
					return;
				}
				else {
					dprintf(D_FULLDEBUG, "'%s' startd key invalidated\n",HashString(hashKey).Value());
				}
			break;
			case INVALIDATE_NEGOTIATOR_ADS:
				dprintf(D_FULLDEBUG, "ODSCollectorPlugin: Received INVALIDATE_NEGOTIATOR_ADS\n");
				if (!makeNegotiatorAdHashKey(hashKey, _ad, NULL)) {
					dprintf(D_FULLDEBUG, "Could not make hashkey -- ignoring ad\n");
					return;
				}
				else {
					dprintf(D_FULLDEBUG, "%s negotiator key invalidated\n",HashString(hashKey).Value());
				}
			break;
			case INVALIDATE_SCHEDD_ADS:
				dprintf(D_FULLDEBUG, "ODSCollectorPlugin: Received INVALIDATE_SCHEDD_ADS\n");
				if (!makeScheddAdHashKey(hashKey, _ad, NULL)) {
					dprintf(D_FULLDEBUG, "Could not make hashkey -- ignoring ad\n");
					return;
				}
				else {
					dprintf(D_FULLDEBUG, "%s scheduler key invalidated\n",HashString(hashKey).Value());
				}
			break;
			case INVALIDATE_GRID_ADS:
				dprintf(D_FULLDEBUG, "ODSCollectorPlugin: Received INVALIDATE_GRID_ADS\n");
				if (!makeGridAdHashKey(hashKey, _ad, NULL)) {
					dprintf(D_FULLDEBUG, "Could not make hashkey -- ignoring ad\n");
					return;
				}
				else {
					dprintf(D_FULLDEBUG, "%s grid key invalidated\n",HashString(hashKey).Value());
				}
			break;
			case INVALIDATE_COLLECTOR_ADS:
				dprintf(D_FULLDEBUG, "ODSCollectorPlugin: Received INVALIDATE_COLLECTOR_ADS\n");
			break;
			default:
				dprintf(D_FULLDEBUG, "ODSCollectorPlugin: Unsupported command: %s\n",
					getCollectorCommandString(command));
		}
	}

};

static ODSCollectorPlugin instance;
