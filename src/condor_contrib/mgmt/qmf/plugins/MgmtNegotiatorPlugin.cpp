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

#include "../condor_negotiator.V6/NegotiatorPlugin.h"

#include "../condor_daemon_core.V6/condor_daemon_core.h"

#include "condor_config.h"

#include "get_daemon_name.h"

#include <qpid/agent/ManagementAgent.h>

#include "NegotiatorObject.h"
#include "broker_utils.h"


struct MgmtNegotiatorPlugin : public Service, NegotiatorPlugin
{
		// ManagementAgent::Singleton cleans up the ManagementAgent
		// instance if there are no ManagementAgent::Singleton's in
		// scope!
	qpid::management::ManagementAgent::Singleton *singleton;

	com::redhat::grid::NegotiatorObject *negotiator;

	void
	initialize()
	{
		char *host;
		char *username;
		char *password;
		char *mechanism;
		int port;
		char *tmp;
		std::string storefile;
		std::string mmName;

		dprintf(D_FULLDEBUG, "MgmtNegotiatorPlugin: Initializing...\n");

		singleton = new qpid::management::ManagementAgent::Singleton();

		qpid::management::ManagementAgent *agent = singleton->getInstance();

		qmf::com::redhat::grid::Negotiator::registerSelf(agent);

		port = param_integer("QMF_BROKER_PORT", 5672);
		if (NULL == (host = param("QMF_BROKER_HOST"))) {
			host = strdup("localhost");
		}

		tmp = param("QMF_STOREFILE");
		if (NULL == tmp) {
			storefile = ".negotiator_storefile";
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

		// pretty much what the daemon does
		tmp = default_daemon_name();
		if (NULL == tmp) {
			mmName = "UNKNOWN NEGOTIATOR HOST";
		} else {
			mmName = tmp;
			free(tmp); tmp = NULL;
		}

		agent->setName("com.redhat.grid","negotiator", mmName.c_str());
		agent->init(std::string(host), port,
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

		negotiator = new com::redhat::grid::NegotiatorObject(agent, mmName.c_str());

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
											   (SocketHandlercpp) (&MgmtNegotiatorPlugin::HandleMgmtSocket),
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

		dprintf(D_FULLDEBUG, "MgmtNegotiatorPlugin: shutting down...\n");
		if (negotiator) {
			delete negotiator;
			negotiator = NULL;
		}
		if (singleton) {
			delete singleton;
			singleton = NULL;
		}
	}

	void
	update(const ClassAd &ad)
	{
		negotiator->update(ad);
	}

	int
	HandleMgmtSocket(/*Service *,*/ Stream *)
	{
		singleton->getInstance()->pollCallbacks();

		return KEEP_STREAM;
	}

};

static MgmtNegotiatorPlugin instance;

