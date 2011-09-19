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

void
processSubmitterStats() {
}

void
processMachineStats() {
}

void
processStatsTimer(Service*) {
    dprintf(D_FULLDEBUG, "processStatsTimer() called\n");
    processSubmitterStats();
    processMachineStats();
}

struct ODSCollectorPlugin : public Service, CollectorPlugin
{
	string m_name;
	string m_ip;
	ODSMongodbOps* m_raw_writer;
    ODSMongodbOps* m_stats_writer;

	void
	initialize()
	{

		dprintf(D_FULLDEBUG, "ODSCollectorPlugin: Initializing...\n");

		m_name = getPoolName();
		m_ip = my_ip_string();

		m_raw_writer = new ODSMongodbOps("condor_raw.ads");
		m_raw_writer->init("localhost");

        m_stats_writer = new ODSMongodbOps("condor_stats.samples");
        m_stats_writer->init("localhost");

        historyInterval = param_integer("POOL_HISTORY_SAMPLING_INTERVAL",60);
        initialDelay=param_integer("UPDATE_INTERVAL",300);

        // Register timer for writing stats to DB
        if (-1 == (historyTimer =
            daemonCore->Register_Timer(
                initialDelay,
                historyInterval,
                (TimerHandler)processStatsTimer,
                "Timer for collecting ODS stats"
            ))) {
            EXCEPT("Failed to register ODS stats timer");
            }
	}

	void
	shutdown()
	{

		dprintf(D_FULLDEBUG, "ODSCollectorPlugin: shutting down...\n");
		delete m_raw_writer;
        delete m_stats_writer;

	}

	void
	update(int command, const ClassAd &ad)
	{
		MyString name;
		AdNameHashKey hashKey;

		// TODO: const hack for make*HashKey and dPrint
		ClassAd* _ad = const_cast<ClassAd *> (&ad);
		BSONObjBuilder key;
		key.append("k",m_name+"#"+time_t_to_String());

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

			m_raw_writer->updateAd(key,_ad);

			break;
        case UPDATE_SUBMITTOR_AD:
            dprintf(D_FULLDEBUG, "ODSCollectorPlugin: Received UPDATE_SUBMITTOR_AD\n");
            if (param_boolean("ODS_IGNORE_UPDATE_SUBMITTOR_AD", FALSE)) {
                dprintf(D_FULLDEBUG, "ODSCollectorPlugin: Configured to ignore UPDATE_SUBMITTOR_AD\n");
                break;
            }

            if (!makeStartdAdHashKey(hashKey, _ad, NULL)) {
                dprintf(D_FULLDEBUG, "Could not make hashkey -- ignoring ad\n");
            }

            m_raw_writer->updateAd(key,_ad);

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

			m_raw_writer->updateAd(key,_ad);

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

			m_raw_writer->updateAd(key,_ad);

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

			m_raw_writer->updateAd(key,_ad);

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
					m_raw_writer->updateAd(key,_ad);
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
