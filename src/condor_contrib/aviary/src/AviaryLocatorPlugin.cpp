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
#include "EndpointObject.h"
#include "LocatorObject.h"

using namespace std;
using namespace aviary::util;
using namespace aviary::transport;
using namespace aviary::locator;

AviaryProvider* provider = NULL;
LocatorObject* locatorObj = NULL;

struct AviaryLocatorPlugin : public Service, CollectorPlugin
{
	typedef HashTable<AdNameHashKey, EndpointObject *> GenericAdHashTable;
	GenericAdHashTable *endpointAds;

	void
	initialize()
	{
		int port;
		char *tmp;
		string collName;

		dprintf(D_FULLDEBUG, "AviaryLocatorPlugin: Initializing...\n");

		endpointAds = new GenericAdHashTable(4096, &adNameHashFunction);

		tmp = param("COLLECTOR_NAME");
		if (NULL == tmp) {
			collName = getPoolName();
		} else {
			collName = tmp;
			free(tmp); tmp = NULL;
		}

		string log_name;
		sprintf(log_name,"aviary_locator.log");
		provider = AviaryProviderFactory::create(log_name, getPoolName(),"LOCATOR", "services/locator/");
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
										   (SocketHandlercpp) ( &AviaryLocatorPlugin::HandleTransportSocket ),
										   "Handler for Aviary Methods.", this))) {
			EXCEPT("Failed to register transport socket");
		}
	}

	// TODO: need this?
	void
	initPublicAd()
	{
// 		publicAd = new ClassAd();
// 		publicAd.SetMyTypeName(GENERIC_ADTYPE);
// 		publicAd.SetTargetTypeName("Locator");
// 
// 		publicAd.Assign(ATTR_NAME, daemonName.c_str());
// 		daemonCore->publish(&publicAd);
	}


	void invalidate_all() {
		endpointAds->clear();
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
		MyString name;
		AdNameHashKey hashKey;
		EndpointObject *endpointObject;

		switch (command) {
		case UPDATE_AD_GENERIC:
			dprintf(D_FULLDEBUG, "AviaryLocatorPlugin: Received UPDATE_AD_GENERIC\n");

			if (!makeGenericAdHashKey(hashKey, ((ClassAd *) &ad))) {
				dprintf(D_FULLDEBUG, "Could not make hashkey -- ignoring ad\n");
			}

			if (endpointAds->lookup(hashKey, endpointObject)) {
				// Key doesn't exist
				endpointObject = new EndpointObject(hashKey.name.Value());

				// TODO: Ignore old value?
				endpointAds->insert(hashKey, endpointObject);
			}

			endpointObject->update(ad);

			break;

		default:
			dprintf(D_FULLDEBUG, "AviaryLocatorPlugin: Unsupported command: %s\n",
					getCollectorCommandString(command));
		}
	}

	void
	invalidate(int command, const ClassAd &ad)
	{
		AdNameHashKey hashKey;
		EndpointObject *endpointObject;

		switch (command) {
			case INVALIDATE_ADS_GENERIC:
				dprintf(D_FULLDEBUG, "AviaryLocatorPlugin: Received INVALIDATE_ADS_GENERIC\n");
				if (!makeStartdAdHashKey(hashKey, ((ClassAd *) &ad))) {
					dprintf(D_FULLDEBUG, "Could not make hashkey -- ignoring ad\n");
					return;
				}
				if (0 == endpointAds->lookup(hashKey, endpointObject)) {
					endpointAds->remove(hashKey);
					delete endpointObject;
				}
				else {
					dprintf(D_FULLDEBUG, "%s endpoint key not found for removal\n",HashString(hashKey).Value());
				}
			break;
			default:
				dprintf(D_FULLDEBUG, "AviaryLocatorPlugin: Unsupported command: %s\n",
					getCollectorCommandString(command));
		}
	}

	int
	HandleTransportSocket(Stream *)
	{
    	string provider_error;
    	if (!provider->processRequest(provider_error)) {
        	dprintf (D_ALWAYS,"Error processing request: %s\n",provider_error.c_str());
    	}
    	return KEEP_STREAM;
	}
};

static AviaryLocatorPlugin instance;
