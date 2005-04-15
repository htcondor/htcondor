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
