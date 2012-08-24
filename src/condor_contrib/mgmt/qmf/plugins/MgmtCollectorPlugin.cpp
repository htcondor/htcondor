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

#include "../condor_collector.V6/CollectorPlugin.h"

#include "hashkey.h"

#include "../condor_collector.V6/collector.h"

#include "../condor_daemon_core.V6/condor_daemon_core.h"

#include "condor_config.h"

#include "SlotObject.h"
#include "NegotiatorObject.h"
#include "SchedulerObject.h"
#include "CollectorObject.h"
#include "GridObject.h"

#include "PoolUtils.h"
#include "broker_utils.h"

//extern DaemonCore *daemonCore;

using namespace std;
using namespace com::redhat::grid;


struct MgmtCollectorPlugin : public Service, CollectorPlugin
{
		// ManagementAgent::Singleton cleans up the ManagementAgent
		// instance if there are no ManagementAgent::Singleton's in
		// scope!
	ManagementAgent::Singleton *singleton;

	typedef HashTable<AdNameHashKey, SlotObject *> SlotHashTable;

	SlotHashTable *startdAds;

	typedef HashTable<AdNameHashKey, NegotiatorObject *> NegotiatorHashTable;

    // Why when there should be only one? Because the CollectorEngine does.
	NegotiatorHashTable *negotiatorAds;

	typedef HashTable<AdNameHashKey, SchedulerObject *> SchedulerHashTable;

	SchedulerHashTable *schedulerAds;

	typedef HashTable<AdNameHashKey, GridObject *> GridHashTable;

	GridHashTable *gridAds;

	CollectorObject *collector;

	void
	initialize()
	{
		char *host;
		char* username;
		char* password;
		char* mechanism;
		int port;
		char *tmp;
		string storefile;
		string collName;

		dprintf(D_FULLDEBUG, "MgmtCollectorPlugin: Initializing...\n");

		singleton = new ManagementAgent::Singleton();

		startdAds = new SlotHashTable(4096, &adNameHashFunction);

		negotiatorAds = new NegotiatorHashTable(3, &adNameHashFunction);

		schedulerAds = new SchedulerHashTable(128, &adNameHashFunction);

		gridAds = new GridHashTable(4096, &adNameHashFunction);

		ManagementAgent *agent = singleton->getInstance();

		Slot::registerSelf(agent);
		Negotiator::registerSelf(agent);
		Scheduler::registerSelf(agent);
		Grid::registerSelf(agent);
		Collector::registerSelf(agent);

		port = param_integer("QMF_BROKER_PORT", 5672);
		if (NULL == (host = param("QMF_BROKER_HOST"))) {
			host = strdup("localhost");
		}

		tmp = param("QMF_STOREFILE");
		if (NULL == tmp) {
			storefile = ".collector_storefile";
		} else {
			storefile = tmp;
			free(tmp); tmp = NULL;
		}

		if (NULL == (username = param("QMF_BROKER_USERNAME")))
		{
			username = strdup("");
		}

		if (NULL == (mechanism = param("QMF_BROKER_AUTH_MECH")))
		{
			mechanism = strdup("ANONYMOUS");
		}

		password = getBrokerPassword();

		tmp = param("COLLECTOR_NAME");
		if (NULL == tmp) {
			collName = GetPoolName();
		} else {
			collName = tmp;
			free(tmp); tmp = NULL;
		}

		agent->setName("com.redhat.grid","collector",collName.c_str());
		agent->init(string(host), port,
					param_integer("QMF_UPDATE_INTERVAL", 10),
					true,
					storefile,
					username,
					password,
					mechanism);

		free(host);
		free(username);
		free(password);
		free(mechanism);

		collector = new CollectorObject(agent, collName.c_str());

		ReliSock *sock = new ReliSock;
		if (!sock) {
			EXCEPT("Failed to allocate Mgmt socket");
		}
		if (!sock->assign(agent->getSignalFd())) {
			EXCEPT("Failed to bind Mgmt socket");
		}
		int index;
		if (-1 == (index =
				   daemonCore->Register_Socket((Stream *) sock,
											   "Mgmt Method Socket",
											   (SocketHandlercpp)
											   (&MgmtCollectorPlugin::HandleMgmtSocket),
											   "Handler for Mgmt Methods.",
											   this))) {
			EXCEPT("Failed to register Mgmt socket");
		}
	}

	void invalidate_all() {
		startdAds->clear();
		negotiatorAds->clear();
		schedulerAds->clear();
		gridAds->clear();
	}

	void
	shutdown()
	{
		if (!param_boolean("QMF_DELETE_ON_SHUTDOWN", true)) {
			return;
		}

		dprintf(D_FULLDEBUG, "MgmtCollectorPlugin: shutting down...\n");

		// clean up our objects here and
		// for the remote console
		invalidate_all();

		if (collector) {
			// delete from the agent
			delete collector;
			collector = NULL;
		}
		if (singleton) {
			delete singleton;
			singleton = NULL;
		}
	}

	void
	update(int command, const ClassAd &ad)
	{
		MyString name;
		AdNameHashKey hashKey;
		SlotObject *slotObject;
		NegotiatorObject *negotiatorObject;
		SchedulerObject *schedulerObject;
		GridObject *gridObject;

		switch (command) {
		case UPDATE_STARTD_AD:
			dprintf(D_FULLDEBUG, "MgmtCollectorPlugin: Received UPDATE_STARTD_AD\n");
			if (param_boolean("QMF_IGNORE_UPDATE_STARTD_AD", true)) {
				dprintf(D_FULLDEBUG, "MgmtCollectorPlugin: Configured to ignore UPDATE_STARTD_AD\n");
				break;
			}

			if (!makeStartdAdHashKey(hashKey, ((ClassAd *) &ad))) {
				dprintf(D_FULLDEBUG, "Could not make hashkey -- ignoring ad\n");
			}

			if (startdAds->lookup(hashKey, slotObject)) {
					// Key doesn't exist
				slotObject = new SlotObject(singleton->getInstance(),
								hashKey.name.Value());

					// Ignore old value, if it existed (returned)
				startdAds->insert(hashKey, slotObject);
			}

			slotObject->update(ad);

			break;
		case UPDATE_NEGOTIATOR_AD:
			dprintf(D_FULLDEBUG, "MgmtCollectorPlugin: Received UPDATE_NEGOTIATOR_AD\n");
			if (param_boolean("QMF_IGNORE_UPDATE_NEGOTIATOR_AD", true)) {
				dprintf(D_FULLDEBUG, "MgmtCollectorPlugin: Configured to ignore UPDATE_NEGOTIATOR_AD\n");
				break;
			}

			if (!makeNegotiatorAdHashKey(hashKey, ((ClassAd *) &ad))) {
				dprintf(D_FULLDEBUG, "Could not make hashkey -- ignoring ad\n");
			}

			if (negotiatorAds->lookup(hashKey, negotiatorObject)) {
					// Key doesn't exist
				if (!ad.LookupString(ATTR_NAME, name)) {
					name = "UNKNOWN";
				}
				name.formatstr("Negotiator: %s", hashKey.name.Value());

				negotiatorObject =
					new NegotiatorObject(singleton->getInstance(),
										 name.Value());

					// Ignore old value, if it existed (returned)
				negotiatorAds->insert(hashKey, negotiatorObject);
			}

			negotiatorObject->update(ad);

			break;
		case UPDATE_SCHEDD_AD:
			dprintf(D_FULLDEBUG, "MgmtCollectorPlugin: Received UPDATE_SCHEDD_AD\n");
			if (param_boolean("QMF_IGNORE_UPDATE_SCHEDD_AD", true)) {
				dprintf(D_FULLDEBUG, "MgmtCollectorPlugin: Configured to ignore UPDATE_SCHEDD_AD\n");
				break;
			}

			if (!makeScheddAdHashKey(hashKey, ((ClassAd *) &ad))) {
				dprintf(D_FULLDEBUG, "Could not make hashkey -- ignoring ad\n");
			}

				// The JobServer constructs a ref to the Scheduler
				// based on this Schedd's name, thus we must construct
				// the Scheduler's id in the same way or a disconnect
				// will occur.
			if (!ad.LookupString(ATTR_NAME, name)) {
				name = "UNKNOWN";
			}

			if (schedulerAds->lookup(hashKey, schedulerObject)) {
					// Key doesn't exist
				schedulerObject =
					new SchedulerObject(singleton->getInstance(),
										 name.Value());

					// Ignore old value, if it existed (returned)
				schedulerAds->insert(hashKey, schedulerObject);
			}

			schedulerObject->update(ad);

			break;
		case UPDATE_GRID_AD:
			dprintf(D_FULLDEBUG, "MgmtCollectorPlugin: Received UPDATE_GRID_AD\n");

			if (!makeGridAdHashKey(hashKey, ((ClassAd *) &ad))) {
				dprintf(D_FULLDEBUG, "Could not make hashkey -- ignoring ad\n");
			}

			if (gridAds->lookup(hashKey, gridObject)) {
					// Key doesn't exist
				gridObject = new GridObject(singleton->getInstance(), hashKey.name.Value());

					// Ignore old value, if it existed (returned)
				gridAds->insert(hashKey, gridObject);
			}

			gridObject->update(ad);

			break;
		case UPDATE_COLLECTOR_AD:
			dprintf(D_FULLDEBUG, "MgmtCollectorPlugin: Received UPDATE_COLLECTOR_AD\n");
				// We could receive collector ads from many
				// collectors, but we only maintain our own. So,
				// ignore all others.
			char *str;
			if (ad.LookupString(ATTR_MY_ADDRESS, &str)) {
				string public_addr(str);
				free(str);

				if (((Collector *)collector->GetManagementObject())->get_MyAddress() == public_addr) {
					collector->update(ad);
				}
			}
			break;
		default:
			dprintf(D_FULLDEBUG, "MgmtCollectorPlugin: Unsupported command: %s\n",
					getCollectorCommandString(command));
		}
	}

	void
	invalidate(int command, const ClassAd &ad)
	{
		AdNameHashKey hashKey;
		SlotObject *slotObject;
		NegotiatorObject *negotaitorObject;
		SchedulerObject *schedulerObject;
		GridObject *gridObject;

		switch (command) {
			case INVALIDATE_STARTD_ADS:
				dprintf(D_FULLDEBUG, "MgmtCollectorPlugin: Received INVALIDATE_STARTD_ADS\n");
				if (!makeStartdAdHashKey(hashKey, ((ClassAd *) &ad))) {
					dprintf(D_FULLDEBUG, "Could not make hashkey -- ignoring ad\n");
					return;
				}
				if (0 == startdAds->lookup(hashKey, slotObject)) {
					startdAds->remove(hashKey);
					delete slotObject;
				}
				else {
					dprintf(D_FULLDEBUG, "%s startd key not found for removal\n",HashString(hashKey).Value());
				}
			break;
			case INVALIDATE_NEGOTIATOR_ADS:
				dprintf(D_FULLDEBUG, "MgmtCollectorPlugin: Received INVALIDATE_NEGOTIATOR_ADS\n");
				if (!makeNegotiatorAdHashKey(hashKey, ((ClassAd *) &ad))) {
					dprintf(D_FULLDEBUG, "Could not make hashkey -- ignoring ad\n");
					return;
				}
				if (0 == negotiatorAds->lookup(hashKey, negotaitorObject)) {
					negotiatorAds->remove(hashKey);
					delete negotaitorObject;
				}
				else {
					dprintf(D_FULLDEBUG, "%s negotiator key not found for removal\n",HashString(hashKey).Value());
				}
			break;
			case INVALIDATE_SCHEDD_ADS:
				dprintf(D_FULLDEBUG, "MgmtCollectorPlugin: Received INVALIDATE_SCHEDD_ADS\n");
				if (!makeScheddAdHashKey(hashKey, ((ClassAd *) &ad))) {
					dprintf(D_FULLDEBUG, "Could not make hashkey -- ignoring ad\n");
					return;
				}
				if (0 == schedulerAds->lookup(hashKey, schedulerObject)) {
					schedulerAds->remove(hashKey);
					delete schedulerObject;
				}
				else {
					dprintf(D_FULLDEBUG, "%s scheduler key not found for removal\n",HashString(hashKey).Value());
				}
			break;
			case INVALIDATE_GRID_ADS:
				dprintf(D_FULLDEBUG, "MgmtCollectorPlugin: Received INVALIDATE_GRID_ADS\n");
				if (!makeGridAdHashKey(hashKey, ((ClassAd *) &ad))) {
					dprintf(D_FULLDEBUG, "Could not make hashkey -- ignoring ad\n");
					return;
				}
				if (0 == gridAds->lookup(hashKey, gridObject)) {
					gridAds->remove(hashKey);
					delete gridObject;
				}
				else {
					dprintf(D_FULLDEBUG, "%s grid key not found for removal\n",HashString(hashKey).Value());
				}
			break;
			case INVALIDATE_COLLECTOR_ADS:
				dprintf(D_FULLDEBUG, "MgmtCollectorPlugin: Received INVALIDATE_COLLECTOR_ADS\n");
			break;
			default:
				dprintf(D_FULLDEBUG, "MgmtCollectorPlugin: Unsupported command: %s\n",
					getCollectorCommandString(command));
		}
	}

	int
	HandleMgmtSocket(/*Service *,*/ Stream *)
	{
		singleton->getInstance()->pollCallbacks();

		return KEEP_STREAM;
	}
};

static MgmtCollectorPlugin instance;
