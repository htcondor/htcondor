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

#ifndef _ENDPOINTOBJECT_H
#define _ENDPOINTOBJECT_H

#include <string>

using namespace std;

namespace aviary {
namespace locator {

struct EndpointStats {
	// properties
	string      CondorPlatform;
    string      CondorVersion;
    int64_t     DaemonStartTime;
    string      Pool;
    string      System;
    string      Machine;
    string      MyAddress;
    string      Name;
	string		EndpointURI;
};

class EndpointObject
{

public:
	EndpointObject(const char *name);
	~EndpointObject();
	void update(const ClassAd &ad);

private:
	string m_name;
	int m_heartbeat;
	EndpointStats m_stats;
};

}} /* aviary::locator */

#endif /* _ENDPOINTOBJECT_H */
