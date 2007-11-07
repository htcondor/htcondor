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
#include "condor_debug.h"
#include "condor_version.h"

int
condor__getPlatformString(struct soap *soap,
						  void *,
						  struct condor__getPlatformStringResponse &result)
{
	const char *platform = CondorPlatform();
	result.response.message =
		(char *) soap_malloc(soap, strlen(platform) + 1);
	ASSERT(result.response.message);
	strcpy(result.response.message, platform);
	result.response.status.code = SUCCESS;
	return SOAP_OK;
}

int
condor__getVersionString(struct soap *soap,
						 void *,
						 struct condor__getVersionStringResponse &result)
{
	const char *version = CondorVersion();
	result.response.message =
		(char *) soap_malloc(soap, strlen(version) + 1);
	ASSERT(result.response.message);
	strcpy(result.response.message, version);
	result.response.status.code = SUCCESS;
	return SOAP_OK;
}
