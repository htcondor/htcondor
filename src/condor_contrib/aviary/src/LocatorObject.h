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
#include "EndpointObject.h"

using namespace std;
using namespace compat_classad;

namespace aviary {
namespace locator {

class LocatorObject
{
public:

	// SOAP-facing method
	bool locate(string& msg);

	// daemonCore-facing method
	void update(EndpointObject* eobj);

    ~LocatorObject();
	static LocatorObject* getInstance();

	// TODO: needed?
	const char* getName() { return m_name.c_str(); }
	const char* getPool() { return m_pool.c_str(); }

private:
    LocatorObject();
	LocatorObject(LocatorObject const&);
	LocatorObject& operator=(LocatorObject const&);

	string m_name;
	string m_pool;

	static LocatorObject* m_instance;

};

}} /* aviary::locator */

#endif /* _LOCATOROBJECT_H */
