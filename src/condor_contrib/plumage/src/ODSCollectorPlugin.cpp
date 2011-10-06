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
        DBClientConnection* conn =  m_ads_conn->m_db_conn;
        conn->ensureIndex(DB_RAW_ADS, BSON( ATTR_MY_TYPE << 1 ));
        auto_ptr<DBClientCursor> cursor = conn->query(DB_RAW_ADS, QUERY( ATTR_MY_TYPE << "Submitter" ) );
        while( cursor->more() ) {
            BSONObj p = cursor->next();
            const char* name = p.getStringField(ATTR_NAME);
            int r = p.getIntField(ATTR_RUNNING_JOBS);
            // TODO: weird...HeldJobs isn't always there in the raw submitter ad
            int h = p.getIntField(ATTR_HELD_JOBS); h = (h>0) ? h : 0;
            int i = p.getIntField(ATTR_IDLE_JOBS);
            dprintf(D_FULLDEBUG, "Submitter %s:\t%s=%d\t%s=%d\t%s=%d\n",
                    name,
                    ATTR_RUNNING_JOBS,r,
                    ATTR_HELD_JOBS,h,
                    ATTR_IDLE_JOBS,i
                   );

            // write record to submitter samples
            conn->ensureIndex(DB_STATS_SAMPLES_SUB, BSON( "ts" << 1 << "sn" << 1 ));
            BSONObjBuilder bob;
            bob << "ts" << DATENOW;
            bob.append("sn",name);
            bob.append("mn",p.getStringField(ATTR_MACHINE));
            bob.append("jr",r);
            bob.append("jh",h);
            bob.append("ji",i);
            conn->insert(DB_STATS_SAMPLES_SUB,bob.obj());
        }
    }

    void
    processMachineStats() {
        dprintf(D_FULLDEBUG, "ODSCollectorPlugin::processMachineStats() called...\n");
        DBClientConnection* conn =  m_ads_conn->m_db_conn;
        conn->ensureIndex(DB_RAW_ADS, BSON( ATTR_MY_TYPE << 1 ));
        auto_ptr<DBClientCursor> cursor = conn->query(DB_RAW_ADS, QUERY( ATTR_MY_TYPE << "Machine" ) );
        while( cursor->more() ) {
            BSONObj p = cursor->next();
            const char* name = p.getStringField(ATTR_NAME);
            const char* arch = p.getStringField(ATTR_ARCH);
            const char* opsys = p.getStringField(ATTR_OPSYS);
            int ki = p.getIntField(ATTR_KEYBOARD_IDLE);
            double la = p.getField(ATTR_LOAD_AVG).Double();
            const char* state = p.getStringField(ATTR_STATE);
            dprintf(D_FULLDEBUG, "Machine %s:\t%s=%s\t%s=%s\t%s=%d\t%s=%f\t%s=%s\n",
                    name,
                    ATTR_ARCH,arch,
                    ATTR_OPSYS,opsys,
                    ATTR_KEYBOARD_IDLE,ki,
                    ATTR_LOAD_AVG,la,
                    ATTR_STATE,state);

            // write record to machine samples
            conn->ensureIndex(DB_STATS_SAMPLES_MACH, BSON( "ts" << 1 << "mn" << 1 ));
            BSONObjBuilder bob;
            bob << "ts" << DATENOW;
            bob.append("mn",name);
            bob.append("ar",arch);
            bob.append("os",opsys);
            bob.append("ki",ki);
            bob.append("la",la);
            bob.append("st",state);
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
			EXCEPT("Failed to initialize DB connection for raw ads");
		}

        m_stats_conn = new ODSMongodbOps(DB_STATS_SAMPLES);
        if (!m_stats_conn->init(dbhost.str())) {
			EXCEPT("Failed to initialize DB connection for stats");
		}

        historyInterval = param_integer("POOL_HISTORY_SAMPLING_INTERVAL",60);
        initialDelay=param_integer("UPDATE_INTERVAL",300);

        // Register timer for writing stats to DB
        if (-1 == (historyTimer =
            daemonCore->Register_Timer(
                initialDelay,
                historyInterval,
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

			if (!makeStartdAdHashKey(hashKey, _ad)) {
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

            if (!makeGenericAdHashKey(hashKey, _ad)) {
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

			if (!makeNegotiatorAdHashKey(hashKey, _ad)) {
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

			if (!makeScheddAdHashKey(hashKey, _ad)) {
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

			if (!makeGridAdHashKey(hashKey, _ad)) {
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
		MyString name,machine;
		AdNameHashKey hashKey;

		// TODO: const hack for make*HashKey
		ClassAd* _ad = const_cast<ClassAd *> (&ad);

		// TODO: ret check this...
		_ad->LookupString(ATTR_NAME,name);

		BSONObjBuilder key;
		key.append(ATTR_NAME,name);

		switch (command) {
			case INVALIDATE_STARTD_ADS:
				dprintf(D_FULLDEBUG, "ODSCollectorPlugin: Received INVALIDATE_STARTD_ADS\n");
				if (!makeStartdAdHashKey(hashKey, _ad)) {
					dprintf(D_FULLDEBUG, "Could not make hashkey -- ignoring ad\n");
					return;
				}
				else {
					dprintf(D_FULLDEBUG, "'%s' startd key invalidated\n",HashString(hashKey).Value());
				}
				m_ads_conn->deleteAd(key);
			break;
			case INVALIDATE_SUBMITTOR_ADS:
				dprintf(D_FULLDEBUG, "ODSCollectorPlugin: Received INVALIDATE_SUBMITTOR_ADS\n");
				if (!makeGenericAdHashKey(hashKey, _ad)) {
					dprintf(D_FULLDEBUG, "Could not make hashkey -- ignoring ad\n");
					return;
				}
				else {
					dprintf(D_FULLDEBUG, "'%s' startd key invalidated\n",HashString(hashKey).Value());
				}

				// TODO: ret check this...
				_ad->LookupString(ATTR_MACHINE,machine);
				key.append(ATTR_MACHINE, machine);

				m_ads_conn->deleteAd(key);
			break;
			case INVALIDATE_NEGOTIATOR_ADS:
				dprintf(D_FULLDEBUG, "ODSCollectorPlugin: Received INVALIDATE_NEGOTIATOR_ADS\n");
				if (!makeNegotiatorAdHashKey(hashKey, _ad)) {
					dprintf(D_FULLDEBUG, "Could not make hashkey -- ignoring ad\n");
					return;
				}
				else {
					dprintf(D_FULLDEBUG, "%s negotiator key invalidated\n",HashString(hashKey).Value());
				}
				m_ads_conn->deleteAd(key);
			break;
			case INVALIDATE_SCHEDD_ADS:
				dprintf(D_FULLDEBUG, "ODSCollectorPlugin: Received INVALIDATE_SCHEDD_ADS\n");
				if (!makeScheddAdHashKey(hashKey, _ad)) {
					dprintf(D_FULLDEBUG, "Could not make hashkey -- ignoring ad\n");
					return;
				}
				else {
					dprintf(D_FULLDEBUG, "%s scheduler key invalidated\n",HashString(hashKey).Value());
				}
				m_ads_conn->deleteAd(key);
			break;
			case INVALIDATE_GRID_ADS:
				dprintf(D_FULLDEBUG, "ODSCollectorPlugin: Received INVALIDATE_GRID_ADS\n");
				if (!makeGridAdHashKey(hashKey, _ad)) {
					dprintf(D_FULLDEBUG, "Could not make hashkey -- ignoring ad\n");
					return;
				}
				else {
					dprintf(D_FULLDEBUG, "%s grid key invalidated\n",HashString(hashKey).Value());
				}
				m_ads_conn->deleteAd(key);
			break;
			case INVALIDATE_COLLECTOR_ADS:
				dprintf(D_FULLDEBUG, "ODSCollectorPlugin: Received INVALIDATE_COLLECTOR_ADS\n");
				m_ads_conn->deleteAd(key);
			break;
			default:
				dprintf(D_FULLDEBUG, "ODSCollectorPlugin: Unsupported command: %s\n",
					getCollectorCommandString(command));
		}
	}

};

static ODSCollectorPlugin instance;
