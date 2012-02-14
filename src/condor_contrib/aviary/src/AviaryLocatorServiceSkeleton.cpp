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
	EndpointVectorType endpoints;
	LocateResponse* response = new LocateResponse;

	bool partials = _locate->getPartialMatches();
	ResourceID* id = _locate->getId();
	ADBResourceTypeEnum res_type = id->getResource()->getResourceTypeEnum();
	switch (res_type) {
		case ResourceType_CUSTOM:
			locator.locate(id->getName(),id->getCustom_type(),partials,endpoints);
		break;
		case ResourceType_ANY:
		case ResourceType_COLLECTOR:
		case ResourceType_MASTER:
		case ResourceType_NEGOTIATOR:
		case ResourceType_SCHEDULER:
		case ResourceType_SLOT:
		default:
			locator.locate(id->getName(),id->getResource()->getResourceType(),partials, endpoints);
		break;
	}
	
	if (endpoints.empty()) {
		response->setStatus(new AviaryCommon::Status(new StatusCodeType("NO_MATCH"),""));
	}
	else {
		for (EndpointVectorType::iterator it = endpoints.begin(); it != endpoints.end(); it++) {
			ResourceLocation* resLoc = new ResourceLocation;
			ResourceID* resId = new ResourceID;
			resId->setName((*it).Name.c_str());
			resId->setPool(locator.getPool());
			resId->setResource(new ResourceType((*it).CustomType.c_str()));
			resLoc->setId(resId);
			resLoc->addLocation(axutil_uri_parse_string(Environment::getEnv(),(*it).EndpointUri.c_str()));
			response->addResources(resLoc);
		}
		response->setStatus(new AviaryCommon::Status(new StatusCodeType(string("OK")),""));
	}

	return response;
}
