#include "condor_common.h"
#include "condor_version.h"

int
condor__getPlatformString(struct soap *soap,
						  void *,
						  struct condor__getPlatformStringResponse &result)
{
	result.response.message = CondorPlatform();
	result.response.status.code = SUCCESS;
	return SOAP_OK;
}

int
condor__getVersionString(struct soap *soap,
						 void *,
						 struct condor__getVersionStringResponse &result)
{
	result.response.message = CondorVersion();
	result.response.status.code = SUCCESS;
	return SOAP_OK;
}
