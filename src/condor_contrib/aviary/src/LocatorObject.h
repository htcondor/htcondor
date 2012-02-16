/*
 * Copyright 2009-2012 Red Hat, Inc.
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

#ifndef _LOCATOROBJECT_H
#define _LOCATOROBJECT_H

// condor includes
#include "condor_common.h"
#include "condor_classad.h"

using namespace std;
using namespace compat_classad;

namespace aviary {
namespace locator {

struct Endpoint {
	// properties
	string      Name;
	string		CustomType;
    string      MyAddress;
	string		EndpointUri;
	int 		missed_updates;
};

typedef vector<Endpoint> EndpointVectorType;
typedef map<string, Endpoint> EndpointMapType;

class LocatorObject
{
public:

	// SOAP-facing method
	void locate(const string& _name, const string& _class, bool partials, 
				EndpointVectorType& matches);

	// daemonCore-facing methods
	void update(const ClassAd& ad);
	void invalidate(const ClassAd& ad);
	void invalidate_all();
	void pruneMissingEndpoints(int max_misses);

	LocatorObject();
    ~LocatorObject();
	string getPool();

private:
	Endpoint createEndpoint(const ClassAd& ad);
	EndpointMapType m_endpoints;

};

extern LocatorObject locator;

}} /* aviary::locator */

#endif /* _LOCATOROBJECT_H */
