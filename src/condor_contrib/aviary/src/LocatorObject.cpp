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

//condor includes
#include "condor_common.h"
#include "condor_config.h"
#include "condor_attributes.h"
#include "condor_debug.h"
#include "stl_string_utils.h"

// C++ includes
// enable for debugging classad to ostream
// watch out for unistd clash
//#include <sstream>

//local includes
#include "LocatorObject.h"
#include "AviaryConversionMacros.h"
#include "AviaryUtils.h"
#include "EndpointPublisher.h"

using namespace std;
using namespace aviary::locator;
using namespace aviary::util;

namespace aviary {
namespace locator {

LocatorObject locator;

}}

LocatorObject::LocatorObject ()
{
    m_publishing = param_boolean("AVIARY_PUBLISH_LOCATION",FALSE);
}

LocatorObject::~LocatorObject()
{
}

bool
LocatorObject::isPublishing() {
    return m_publishing;
}

string 
LocatorObject::getPool() {
	return getPoolName();
}

void
LocatorObject::locate(const string& name, const string& major, const string& minor, bool partials, 
					  EndpointVectorType& matches)
{
	dprintf(D_FULLDEBUG,"LocatorObject::locate: %s/%s/%s\n",name.c_str(),major.c_str(),minor.c_str());
	for (EndpointMapType::iterator it = m_endpoints.begin(); it != m_endpoints.end(); it++) {
		if (major == (*it).second.MajorType || major == "ANY") {
			if (minor == (*it).second.MinorType || minor.empty()) {
				if (!partials && name == (*it).second.Name) {
					matches.push_back((*it).second);
				}
				else if (string::npos != (*it).second.Name.find(name)) {
					matches.push_back((*it).second);
				}
			}
		}
	}
}

void
LocatorObject::update (const ClassAd& ad)
{
	string uri;

	if (!ad.LookupString(ENDPOINT_URI,uri)){
		dprintf(D_ALWAYS,"LocatorObject: update ad doesn't contain %s attribute!\n",ENDPOINT_URI);
		return;
	}

	EndpointMapType::iterator it = m_endpoints.find(uri.c_str());
	if (it == m_endpoints.end()) {
		m_endpoints[uri] = createEndpoint(ad);
		dprintf(D_FULLDEBUG,"LocatorObject: added endpoint '%s'\n",uri.c_str());
	}
	else {
		// found it so reset its heartbeat flag
		(*it).second.missed_updates = 0;
	}
	
	// debug
	if (DebugFlags & D_FULLDEBUG) {
		const_cast<ClassAd*>(&ad)->dPrint(D_FULLDEBUG|D_NOHEADER);
	}
}

void
LocatorObject::invalidate(const ClassAd& ad)
{
	string uri;

	if (!ad.LookupString(ENDPOINT_URI,uri)){
		dprintf(D_ALWAYS,"LocatorObject: invalidate ad doesn't contain %s attribute!\n",ENDPOINT_URI);
		return;
	}

	EndpointMapType::iterator it = m_endpoints.find(uri);
	if (it != m_endpoints.end()) {
		dprintf(D_FULLDEBUG,"LocatorObject: removing endpoint '%s'\n",(*it).first.c_str());
		m_endpoints.erase(it);
	}
}

void
LocatorObject::invalidate_all() {
	m_endpoints.clear();
}

Endpoint
LocatorObject::createEndpoint(const compat_classad::ClassAd& ad) {

	Endpoint m_stats;
	MGMT_DECLARATIONS;

	STRING(MyAddress);
	STRING(Name);
	STRING(EndpointUri);
	STRING(MajorType);
	STRING(MinorType);

	m_stats.missed_updates = 0;

	return m_stats;

}

void
LocatorObject::pruneMissingEndpoints(int max_misses) {
	// walk our collection of endpoints, marking & removing as needed
	for (EndpointMapType::iterator it = m_endpoints.begin(); m_endpoints.end() != it; it++) {
		(*it).second.missed_updates++;
		if ((*it).second.missed_updates > max_misses) {
			dprintf(D_FULLDEBUG,"LocatorObject: pruning endpoint '%s'\n",(*it).first.c_str());
			m_endpoints.erase(it);
		}
	}
}