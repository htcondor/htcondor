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

using namespace std;
using namespace aviary::locator;
using namespace aviary::util;

LocatorObject* LocatorObject::m_instance = NULL;

LocatorObject::LocatorObject ()
{
	m_name = getScheddName();
	m_pool = getPoolName();
}

LocatorObject::~LocatorObject()
{
}

LocatorObject* LocatorObject::getInstance()
{
    if (!m_instance) {
        m_instance = new LocatorObject();
    }
    return m_instance;
}

bool
LocatorObject::locate(string& /*error*/)
{
	// TODO: impl
	return true;
}

void
LocatorObject::update ( EndpointObject* eobj )
{
	// TODO: impl
}
