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
#include "../condor_startd.V6/StartdPlugin.h"
#include "hashkey.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_config.h"
#include "get_daemon_name.h"

// Global from the condor_startd, it's name
extern char * Name;

using namespace std;
using namespace mongo;
using namespace plumage::etl;

struct ODSStartdPlugin : public Service, StartdPlugin
{
    string startd_name;
    ODSMongodbOps* writer;

	void
	initialize()
	{

		dprintf(D_FULLDEBUG, "ODSStartdPlugin: Initializing...\n");

		startd_name = default_daemon_name();
		if( Name ) {
			startd_name = Name;
		}
		
		writer = new ODSMongodbOps("condor.startd");
        writer->init("localhost");

	}

	void
	shutdown()
	{
		dprintf(D_FULLDEBUG, "ODSStartdPlugin: shutting down...\n");
        delete writer;
	}
	
	void invalidate(const ClassAd*) {
        // TODO: what does it mean to invalidate the slot ads here?
    }

	void
	update(const ClassAd *ad, const ClassAd *)
	{
        // write a new record
        BSONObjBuilder key;
        key.append("k",startd_name+"#"+time_t_to_String());
		writer->updateAd(key,const_cast<ClassAd*>(ad));
	}

};

static ODSStartdPlugin instance;
