/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

#include "condor_common.h"

#include "condor_classad.h"
#include "condor_daemon_core.h"
#include "condor_version.h"
#include "condor_attributes.h"

#include "condorSoapshell.nsmap"
#include "../condor_c++_util/soap_helpers.cpp"

extern ClassAd* process_request(const ClassAd*);

int condor__runCommandWithString(struct soap *soap,
		char *adSerializedToString, 
		struct condor__runCommandWithStringResponse  &result )
{

	ClassAd inputAd;

	if ( inputAd.initFromString(adSerializedToString) == false ) {
		result.response.status.code = FAIL;
		result.response.status.message = "Failed to parse request ad";
		return SOAP_OK;
	}

	ClassAd * resultAd = process_request(&inputAd);

	MyString temp;
	resultAd->sPrint(temp);
	result.response.message = (char * ) soap_malloc(soap, temp.Length()+1);
	strcpy(result.response.message,temp.Value());
	result.response.status.code = SUCCESS;

	return SOAP_OK;
}


int condor__runCommandWithClassAd(struct soap *soap,
		struct condor__ClassAdStruct adStruct,
		struct condor__runCommandWithClassAdResponse &result)
{

	dprintf(D_ALWAYS, "Entering condor__runCommandWithClassAd...\n");

	ClassAd *ad = new ClassAd;
	if (!ad) {
		result.response.status.code = FAIL;
		result.response.status.message = "Service has run out of memory, sorry";
		return SOAP_OK;
	}

	if (!convert_adStruct_to_ad(soap, ad, &adStruct)) {
		dprintf(D_FULLDEBUG,
				"Failed to convert ClassAdStruct to ClassAd.\n");
		result.response.status.code = FAIL;
		result.response.status.message = "Failed to parse the incoming ClassAdStruct";
		delete ad;
		return SOAP_OK;
	}

	ClassAd * resultAd = process_request(ad);

	if (!convert_ad_to_adStruct(soap,
								resultAd,
								&result.response.classAd,
								false)) 
	{
		dprintf(D_FULLDEBUG,
				"ERROR ad to adStructArray failed!\n");
		result.response.status.code = FAIL;
		result.response.status.message = "Failed to serialize job ad";
	} else {
		result.response.status.code = SUCCESS;
		result.response.status.message = "Results returned in classad";
	}

	if (ad) delete ad;
	if (resultAd) delete resultAd;

	dprintf(D_ALWAYS, "Leaving condor__runCommandWithClassAd...\n");

	return SOAP_OK;
}



///////////////////////////////////////////////////////////////////////////////
// TODO : This should move into daemonCore once we figure out how we wanna link
///////////////////////////////////////////////////////////////////////////////

#include "soap_daemon_core.cpp"

