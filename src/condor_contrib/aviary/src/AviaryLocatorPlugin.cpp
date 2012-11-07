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

// condor includes
#include "condor_common.h"
#include "condor_config.h"
#include "../condor_collector.V6/CollectorPlugin.h"
#include "hashkey.h"
#include "../condor_collector.V6/collector.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

// local includes
#include "AviaryUtils.h"
#include "AviaryProvider.h"
#include "LocatorObject.h"
#include "AviaryLocatorServiceSkeleton.h"

using namespace std;
using namespace aviary::util;
using namespace aviary::transport;
using namespace aviary::locator;

AviaryProvider* provider = NULL;

struct AviaryLocatorPlugin : public Service, CollectorPlugin
{

	void
	initialize()
	{
		char *tmp;
		string collName;

		dprintf(D_FULLDEBUG, "AviaryLocatorPlugin: Initializing...\n");

		tmp = param("COLLECTOR_NAME");
		if (NULL == tmp) {
			collName = getPoolName();
		} else {
			collName = tmp;
			free(tmp); tmp = NULL;
		}

		string log_name("aviary_locator.log");
		string id_name("locator"); id_name+=SEPARATOR; id_name+=getPoolName();
		provider = AviaryProviderFactory::create(log_name,id_name,CUSTOM,LOCATOR, "services/locator/");
		if (!provider) {
			EXCEPT("Unable to configure AviaryProvider. Exiting...");
		}

		ReliSock *sock = new ReliSock;
		if (!sock) {
			EXCEPT("Failed to allocate transport socket");
		}

		if (!sock->assign(provider->getListenerSocket())) {
			EXCEPT("Failed to bind transport socket");
		}
		int index;
		if (-1 == (index =
				daemonCore->Register_Socket((Stream *) sock,
											"Aviary Method Socket",
										   (SocketHandlercpp) ( &AviaryLocatorPlugin::handleTransportSocket ),
										   "Handler for Aviary Methods.", this))) {
			EXCEPT("Failed to register transport socket");
		}

		int pruning_interval = param_integer("AVIARY_LOCATOR_PRUNE_INTERVAL",20);
		if (-1 == (index = daemonCore->Register_Timer(
			pruning_interval,pruning_interval*2,
							(TimerHandlercpp)(&AviaryLocatorPlugin::handleTimerCallback),
							"Timer for pruning unresponsive endpoints", this))) {
        	EXCEPT("Failed to register pruning timer");
        }
	}

	void invalidate_all() {
		locator.invalidateAll();
	}

	void
	shutdown()
	{
		dprintf(D_FULLDEBUG, "AviaryLocatorPlugin: shutting down...\n");
		invalidate_all();
	}

	void
	update(int command, const ClassAd &ad)
	{
		string generic_target_name;

		switch (command) {
		case UPDATE_AD_GENERIC:
			dprintf(D_FULLDEBUG, "AviaryLocatorPlugin: Received UPDATE_AD_GENERIC\n");
			if (ad.LookupString(ATTR_TARGET_TYPE,generic_target_name)) {
				if (generic_target_name != ENDPOINT) {
					return;
				}
				locator.update(ad);
			}
			break;

		default:
			dprintf(D_FULLDEBUG, "AviaryLocatorPlugin: Unsupported command: %s\n",
					getCollectorCommandString(command));
		}
	}

	void
	invalidate(int command, const ClassAd &ad)
	{
		string generic_target_name;

		switch (command) {
			case INVALIDATE_ADS_GENERIC:
				dprintf(D_FULLDEBUG, "AviaryLocatorPlugin: Received INVALIDATE_ADS_GENERIC\n");
				if (ad.LookupString(ATTR_TARGET_TYPE,generic_target_name)) {
					if (generic_target_name != ENDPOINT) {
						return;
					}
					locator.invalidate(ad);
				}
			break;
			default:
				dprintf(D_FULLDEBUG, "AviaryLocatorPlugin: Unsupported command: %s\n",
					getCollectorCommandString(command));
		}
	}

	int
	handleTransportSocket(Stream *)
	{
    	string provider_error;
    	if (!provider->processRequest(provider_error)) {
        	dprintf (D_ALWAYS,"Error processing request: %s\n",provider_error.c_str());
    	}
    	return KEEP_STREAM;
	}

	void
	handleTimerCallback(Service *)
	{
		int max_misses = param_integer("AVIARY_LOCATOR_MISSED_UPDATES",2);
		locator.pruneMissingEndpoints(max_misses);
	}
};

static AviaryLocatorPlugin instance;
