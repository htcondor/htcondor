/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
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
