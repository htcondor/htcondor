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

#include "../condor_master.V6/MasterPlugin.h"
#include "../condor_master.V6/master.h"

#include "condor_classad.h"

#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "get_daemon_name.h"

#include "condor_config.h"

#include "qpid/management/Manageable.h"
#include "qpid/management/ManagementObject.h"
#include "qpid/agent/ManagementAgent.h"

#include "MasterObject.h"

#include "Master.h"

#include "ArgsMasterStop.h"
#include "ArgsMasterStart.h"

#include "PoolUtils.h"
#include "broker_utils.h"

#include "MgmtConversionMacros.h"

extern DaemonCore *daemonCore;
extern char	*MasterName;

extern class Daemons daemons;

using namespace std;
using namespace qpid::management;
using namespace qmf::com::redhat::grid;


struct MgmtMasterPlugin : public Service, MasterPlugin
{
		// ManagementAgent::Singleton cleans up the ManagementAgent
		// instance if there are no ManagementAgent::Singleton's in
		// scope!
	ManagementAgent::Singleton *singleton;

	com::redhat::grid::MasterObject *master;


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

		dprintf(D_FULLDEBUG, "MgmtMasterPlugin: Initializing...\n");

		singleton = new ManagementAgent::Singleton();

		ManagementAgent *agent = singleton->getInstance();

		Master::registerSelf(agent);

		port = param_integer("QMF_BROKER_PORT", 5672);
		if (NULL == (host = param("QMF_BROKER_HOST"))) {
			host = strdup("localhost");
		}

		tmp = param("QMF_STOREFILE");
		if (NULL == tmp) {
			storefile = ".master_storefile";
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


		// crib some stuff from master daemon
		char* default_name = NULL;
		if (MasterName) {
			default_name = MasterName;
		} else {
			default_name = default_daemon_name();
			if( ! default_name ) {
				EXCEPT( "default_daemon_name() returned NULL" );
			}
		}

		agent->setName("com.redhat.grid","master", default_name);
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

		master = new com::redhat::grid::MasterObject(agent, default_name);
		delete [] default_name;

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
											   &MgmtMasterPlugin::HandleMgmtSocket,
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

		dprintf(D_FULLDEBUG, "MgmtMasterPlugin: shutting down...\n");

		// remove from agent
		if (master) {
			delete master;
			master = NULL;
		}
		if (singleton) {
			delete singleton;
			singleton = NULL;
		}
	}

	void
	update(const ClassAd *ad)
	{
		dprintf(D_FULLDEBUG, "MgmtMasterPlugin: calling update\n");
		master->update(*ad);
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

static MgmtMasterPlugin instance;

#if defined(WIN32)
int load_master_mgmt(void) {
	return 0;
}
#endif
