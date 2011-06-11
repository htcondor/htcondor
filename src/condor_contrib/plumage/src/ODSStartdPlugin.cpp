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
#include "../condor_startd.V6/StartdPlugin.h"
#include "hashkey.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_config.h"
#include "get_daemon_name.h"

// Global from the condor_startd, it's name
extern char * Name;

using namespace std;
using namespace plumage::etl;

ODSMongodbWriter* writer = NULL;

struct ODSStartdPlugin : public Service, StartdPlugin
{

	void
	initialize()
	{

		dprintf(D_FULLDEBUG, "ODSStartdPlugin: Initializing...\n");

		std::string startd_name = default_daemon_name();
		if( Name ) {
			startd_name = Name;
		}
		
		writer = new ODSMongodbWriter("condor.startd");
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
		writer->writeClassAd("",const_cast<ClassAd*>(ad));
	}

};

static ODSStartdPlugin instance;
