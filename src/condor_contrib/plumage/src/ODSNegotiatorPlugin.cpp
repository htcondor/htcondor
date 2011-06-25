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

// seems boost meddles with assert defs
#include "assert.h"
#include "condor_config.h"
#include "../condor_negotiator.V6/NegotiatorPlugin.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "get_daemon_name.h"

using namespace std;
using namespace mongo;
using namespace plumage::etl;

struct ODSNegotiatorPlugin : public Service, NegotiatorPlugin
{
    
    string n_name;
    ODSMongodbOps* writer;

	void
	initialize()
	{
		char *tmp;

		dprintf(D_FULLDEBUG, "ODSNegotiatorPlugin: Initializing...\n");

		// pretty much what the daemon does
		tmp = default_daemon_name();
		if (NULL == tmp) {
			n_name = "UNKNOWN NEGOTIATOR HOST";
		} else {
			n_name = tmp;
			free(tmp); tmp = NULL;
		}
		
		writer = new ODSMongodbOps("condor.negotiator");
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
        // write a new record
        BSONObjBuilder key;
        key.append("k",n_name+"#"+time_t_to_String());
		writer->updateAd(key,const_cast<ClassAd*>(&ad));
	}

};

static ODSNegotiatorPlugin instance;
