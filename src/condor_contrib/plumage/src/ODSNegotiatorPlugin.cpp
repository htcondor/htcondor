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
#include "ODSMongodbRW.h"

// seems boost meddles with assert defs
#include "assert.h"
#include "condor_config.h"
#include "../condor_negotiator.V6/NegotiatorPlugin.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "get_daemon_name.h"

using namespace plumage::etl;

ODSMongodbWriter* writer = NULL;

struct ODSNegotiatorPlugin : public Service, NegotiatorPlugin
{

	void
	initialize()
	{
		char *tmp;
		std::string mmName;

		dprintf(D_FULLDEBUG, "ODSNegotiatorPlugin: Initializing...\n");

		// pretty much what the daemon does
		tmp = default_daemon_name();
		if (NULL == tmp) {
			mmName = "UNKNOWN NEGOTIATOR HOST";
		} else {
			mmName = tmp;
			free(tmp); tmp = NULL;
		}
		
		writer = new ODSMongodbWriter("condor.negotiator");
        writer->init("localhost");

	}

	void
	shutdown()
	{
		dprintf(D_FULLDEBUG, "ODSNegotiatorPlugin: shutting down...\n");
        delete writer;
	}

	void
	update(const ClassAd &ad)
	{
		writer->writeClassAd("",const_cast<ClassAd*>(&ad));
	}

};

static ODSNegotiatorPlugin instance;
