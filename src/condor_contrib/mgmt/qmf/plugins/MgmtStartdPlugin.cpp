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

#include "../condor_startd.V6/StartdPlugin.h"

#include "hashkey.h"

#include "../condor_daemon_core.V6/condor_daemon_core.h"

#include "condor_config.h"
#include "get_daemon_name.h"

#include "SlotObject.h"
#include "broker_utils.h"

// Global from the condor_startd, it's name
extern char * Name;

using namespace std;
using namespace com::redhat::grid;


struct MgmtStartdPlugin : public Service, StartdPlugin
{
		// ManagementAgent::Singleton cleans up the ManagementAgent
		// instance if there are no ManagementAgent::Singleton's in
		// scope!
	ManagementAgent::Singleton *singleton;

	typedef HashTable<AdNameHashKey, SlotObject *> SlotHashTable;

	SlotHashTable *startdAds;

	void
	initialize()
	{
		char *host;
		char *username;
		char *password;
		char *mechanism;
		int port;
		char *tmp;
		string storefile;

		dprintf(D_FULLDEBUG, "MgmtStartdPlugin: Initializing...\n");

		singleton = new ManagementAgent::Singleton();

		startdAds = new SlotHashTable(4096, &adNameHashFunction);

		ManagementAgent *agent = singleton->getInstance();

		Slot::registerSelf(agent);

		port = param_integer("QMF_BROKER_PORT", 5672);
		if (NULL == (host = param("QMF_BROKER_HOST"))) {
			host = strdup("localhost");
		}

		tmp = param("QMF_STOREFILE");
		if (NULL == tmp) {
			storefile = ".startd_storefile";
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

		std::string startd_name = default_daemon_name();
		if( Name ) {
			startd_name = Name;
		}

		agent->setName("com.redhat.grid","slot",startd_name.c_str());
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
											   &MgmtStartdPlugin::HandleMgmtSocket,
											   "Handler for Mgmt Methods.",
											   this))) {
			EXCEPT("Failed to register Mgmt socket");
		}
	}

	void
	shutdown()
	{
		if (!param_boolean("QMF_DELETE_ON_SHUTDOWN", true)) {
			return;
		}

		dprintf(D_FULLDEBUG, "MgmtStartdPlugin: shutting down...\n");

		// invalidate will clean up qmf SlotObjects

		if (singleton) {
			delete singleton;
			singleton = NULL;
		}
	}

	void
	update(const ClassAd *ad, const ClassAd *)
	{
		AdNameHashKey hashKey;
		SlotObject *slotObject;

		if (!makeStartdAdHashKey(hashKey, ((ClassAd *) ad))) {
			dprintf(D_FULLDEBUG, "Could not make hashkey -- ignoring ad\n");
			return;
		}

		if (startdAds->lookup(hashKey, slotObject)) {
				// Key doesn't exist
			slotObject = new SlotObject(singleton->getInstance(), hashKey.name.Value());

				// Ignore old value, if it existed (returned)
			startdAds->insert(hashKey, slotObject);
		}

		slotObject->update(*ad);
	}

	void
	invalidate(const ClassAd *ad)
	{
		AdNameHashKey hashKey;
		SlotObject *slotObject;

		if (!makeStartdAdHashKey(hashKey, ((ClassAd *) ad))) {
			dprintf(D_FULLDEBUG, "Could not make hashkey -- ignoring ad\n");
			return;
		}

		// find it and throw it away
		if (0 == startdAds->lookup(hashKey, slotObject)) {
			startdAds->remove(hashKey);
			delete slotObject;
		}
		else {
			dprintf(D_FULLDEBUG, "%s startd key not found for removal\n",HashString(hashKey).Value());
		}
	}

	// this needs an exact match signature for VC++
#ifdef WIN32
	int
	HandleMgmtSocket(Stream *)
#else
	int
	HandleMgmtSocket(Service *, Stream *)
#endif
	{
		singleton->getInstance()->pollCallbacks();

		return KEEP_STREAM;
	}
};

static MgmtStartdPlugin instance;

#if defined(WIN32)
int load_startd_mgmt(void) {
	return 0;
}
#endif
