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
#include "condor_attributes.h"

// c++ includes
#include <cmath>
#include <limits>

// local includes
#include "ODSUtils.h"
#include "ODSMongodbOps.h"

using namespace std;
using namespace plumage::etl;
using namespace mongo;

static MyString formatter;

string
plumage::util::getPoolName()
{
	std::string poolName;
	char *tmp;

	tmp = param("COLLECTOR_HOST");
	if (!tmp) {
		tmp = strdup("NO COLLECTOR_HOST, NOT GOOD");
	}
	poolName = tmp;
	free(tmp); tmp = NULL;

	return poolName;
}

// insert accountant quota, etc. with 
// the precision we appear to see from userprio
// utility to manage float precision to
// ClassAd serialization standard
bool areSame(double a, double b) {
    return fabs(a - b) < numeric_limits<double>::epsilon();
}

const char* 
plumage::util::formatReal(double x) {
    if (areSame(x,0.0) || areSame(x,1.0)) {
        formatter.formatstr("%.1G", x);
    }
    else {
        formatter.formatstr("%.6G", x);
    }
    return formatter.Value();
}

// cleans up the quoted values from the job log reader
string 
plumage::util::trimQuotes(const char* str) {
    string val = str;

    size_t endpos = val.find_last_not_of("\\\"");
    if( string::npos != endpos ) {
        val = val.substr( 0, endpos+1 );
    }
    size_t startpos = val.find_first_not_of("\\\"");
    if( string::npos != startpos ) {
        val = val.substr( startpos );
    }

    return val;
}

void
plumage::util::addJobIndices(ODSMongodbOps* ops) {
    // set our indices of interest for the tools
    ops->addIndex(BSON( ATTR_CLUSTER_ID << 1 ).getOwned());
    ops->addIndex(BSON( ATTR_CLUSTER_ID << 1 << ATTR_PROC_ID << 1 ).getOwned());
    ops->addIndex(BSON( ATTR_GLOBAL_JOB_ID << 1 ).getOwned());
    ops->addIndex(BSON( ATTR_OWNER << 1 ).getOwned());
    ops->addIndex(BSON( ATTR_JOB_SUBMISSION << 1 ).getOwned());
    ops->addIndex(BSON( ATTR_Q_DATE << 1 ).getOwned());
}

HostAndPort 
plumage::util::getDbHostPort(const char* host_param, const char* port_param) {
    char* tmp = NULL;
    string db_host;
    int db_port;

    if (NULL != (tmp = param(host_param))) {
        db_host = tmp;
        free (tmp);
    }
    else {
        db_host = "localhost";
    }

    param_integer(port_param,db_port,true,27017);

    return HostAndPort(db_host,db_port); 
}