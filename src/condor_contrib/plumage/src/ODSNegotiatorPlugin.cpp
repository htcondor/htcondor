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

// mongodb-devel includes
#include <mongo/client/dbclient.h>

// seems boost meddles with assert defs
#include "assert.h"
#include "condor_config.h"
#include "../condor_negotiator.V6/NegotiatorPlugin.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "get_daemon_name.h"

using namespace mongo;
DBClientConnection c;
std::string db_name("condor.negotiator");

void writeJobAd(ClassAd* ad) {
    clock_t start, end;
    double elapsed;
    ExprTree *expr;
    const char *name;
    ad->ResetExpr();
    
    BSONObjBuilder b;
    start = clock();
    while (ad->NextExpr(name,expr)) {        
        b.append(name,ExprTreeToString(expr));
    }
    try {
        c.insert(db_name, b.obj());
    }
    catch(DBException& e) {
        cout << "caught DBException " << e.toString() << endl;
    }
    end = clock();
    elapsed = ((float) (end - start)) / CLOCKS_PER_SEC;
    std:string last_err = c.getLastError();
    if (!last_err.empty()) {
        printf("getLastError: %s\n",last_err.c_str());
    }
    printf("Time elapsed: %1.9f sec\n",elapsed);
}


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
		
		c.connect("localhost");
	}

	void
	shutdown()
	{
		dprintf(D_FULLDEBUG, "ODSNegotiatorPlugin: shutting down...\n");
	}

	void
	update(const ClassAd &ad)
	{
		writeJobAd(const_cast<ClassAd*>(&ad));
	}

};

static ODSNegotiatorPlugin instance;

