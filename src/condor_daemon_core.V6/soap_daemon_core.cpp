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

int
condor__getInfoAd(struct soap *soap,
				  void *,
				  struct condor__getInfoAdResponse & result)
{
	char* todd = "Todd A Tannenbaum";

	result.response.classAd.__size = 3;
	result.response.classAd.__ptr = (struct condor__ClassAdStructAttr *)soap_malloc(soap,3 * sizeof(struct condor__ClassAdStructAttr));

	result.response.classAd.__ptr[0].name = "Name";
	result.response.classAd.__ptr[0].type = STRING_ATTR;
	result.response.classAd.__ptr[0].value = todd;

	result.response.classAd.__ptr[1].name = "Age";
	result.response.classAd.__ptr[1].type = INTEGER_ATTR;
	result.response.classAd.__ptr[1].value = "35";
	int* age = (int*)soap_malloc(soap,sizeof(int));
	*age = 35;

	result.response.classAd.__ptr[2].name = "Friend";
	result.response.classAd.__ptr[2].type = STRING_ATTR;
	result.response.classAd.__ptr[2].value = todd;

	result.response.status.code = SUCCESS;

	return SOAP_OK;
}
