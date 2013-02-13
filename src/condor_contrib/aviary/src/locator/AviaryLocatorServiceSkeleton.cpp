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

// the implementation methods for AviaryLocatorService methods

// condor includes
#include "condor_common.h"

// axis2 includes
#include "Environment.h"

// local includes
#include "LocatorObject.h"
#include "AviaryLocatorServiceSkeleton.h"
#include <AviaryLocator_Locate.h>
#include <AviaryLocator_LocateResponse.h>

using namespace std;
using namespace wso2wsf;
using namespace AviaryCommon;
using namespace AviaryLocator;
using namespace aviary::locator;

LocateResponse* AviaryLocatorServiceSkeleton::locate(wso2wsf::MessageContext* /*outCtx*/ , Locate* _locate)
{
	string error;
	EndpointSetType endpoints;
	LocateResponse* response = new LocateResponse;

    if (!locator.isPublishing()) {
        response->setStatus(new AviaryCommon::Status(new StatusCodeType("UNAVAILABLE"),
            "Locator service not configured for publishing. Check value of AVIARY_PUBLISH_LOCATION!"));
        return response;
    }

	// happily assume partial name matches
	bool partials = _locate->isPartialMatchesNil() || _locate->getPartialMatches();
	ResourceID* id = _locate->getId();
	locator.locate(id->getName(),id->getResource()->getResourceType(),id->getSub_type(),partials,endpoints);

	if (endpoints.empty()) {
		response->setStatus(new AviaryCommon::Status(new StatusCodeType("NO_MATCH"),""));
	}
	else {
		for (EndpointSetType::iterator it = endpoints.begin(); it != endpoints.end(); it++) {
			ResourceLocation* resLoc = new ResourceLocation;
			ResourceID* resId = new ResourceID;
			if (partials) {
				// split off separators
				resId->setName((*it).Name.substr((*it).Name.find(aviary::locator::SEPARATOR)+1));
			}
			else {
				resId->setName((*it).Name);
			}
			resId->setPool(locator.getPool());
			resId->setResource(new ResourceType((*it).MajorType.c_str()));
			resId->setSub_type((*it).MinorType.c_str());
			resLoc->setId(resId);
			resLoc->addLocation(axutil_uri_parse_string(Environment::getEnv(),(*it).EndpointUri.c_str()));
			response->addResources(resLoc);
		}
		response->setStatus(new AviaryCommon::Status(new StatusCodeType(string("OK")),""));
	}

	return response;
}
