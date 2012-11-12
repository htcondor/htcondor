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

const char SEPARATOR[] = "#";

struct Endpoint {
	// properties
	string      Name;
	string		MajorType;
	string		MinorType;
    string      Machine;
    string      MyAddress;
	string		EndpointUri;
	int 		missed_updates;

    // this is how we decided what is replaceable
    bool operator==(const Endpoint& ep) {
        return  (this->MyAddress == ep.MyAddress) && 
                (this->Name == ep.Name);
    };

    friend std::stringstream& operator<< (std::stringstream &out, Endpoint& ep)
    {
        out << ep.Name << "/" << ep.MyAddress;
        return out;
    };

};

struct CompEndpoints {
    bool operator()(const Endpoint& a, const Endpoint& b) const {
        return a.Name < b.Name;
    }
};

typedef set<Endpoint,CompEndpoints> EndpointSetType;
typedef map<string, Endpoint> EndpointMapType;

class LocatorObject
{
public:

	// SOAP-facing method
	void locate(const string& name, const string& major, const string& minor, bool partials, 
				EndpointSetType& matches);

	// daemonCore-facing methods
	void update(const ClassAd& ad);
	void invalidate(const ClassAd& ad);
	void invalidateAll();
	void pruneMissingEndpoints(int max_misses);
    bool isPublishing();

	LocatorObject();
    ~LocatorObject();
	string getPool();

private:
	Endpoint createEndpoint(const ClassAd& ad);
	EndpointMapType m_endpoints;
    bool m_publishing;

};

extern LocatorObject locator;

}} /* aviary::locator */

#endif /* _LOCATOROBJECT_H */
