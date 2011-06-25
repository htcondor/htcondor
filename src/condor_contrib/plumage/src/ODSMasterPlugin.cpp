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
#include "../condor_master.V6/MasterPlugin.h"
#include "../condor_master.V6/master.h"
#include "condor_classad.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "get_daemon_name.h"

extern DaemonCore *daemonCore;
extern char	*MasterName;

using namespace std;
using namespace mongo;
using namespace plumage::etl;

struct ODSMasterPlugin : public Service, MasterPlugin
{
    string default_name;
    ODSMongodbOps* writer;
    
	void
	initialize()
	{
		char *tmp;

		dprintf(D_FULLDEBUG, "ODSMasterPlugin: Initializing...\n");

		// crib some stuff from master daemon
		if (MasterName) {
			default_name = MasterName;
		} else {
			default_name = default_daemon_name();
			if( default_name.empty() ) {
				EXCEPT( "default_daemon_name() returned NULL" );
			}
		}

        writer = new ODSMongodbOps("condor.master");
        writer->init("localhost");
	}

	void
	shutdown()
	{
		dprintf(D_FULLDEBUG, "ODSMasterPlugin: shutting down...\n");
        delete writer;
	}

	void
	update(const ClassAd *ad)
	{
        // write a new record
        BSONObjBuilder key;
        key.append("k",default_name+"#"+time_t_to_String());
		writer->updateAd(key,const_cast<ClassAd*>(ad));
	}

};

static ODSMasterPlugin instance;
