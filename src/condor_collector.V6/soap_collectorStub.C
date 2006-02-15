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
#include "condor_classad.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

// Things to include for the stubs
#include "condor_version.h"
#include "collector.h"
#include "condor_attributes.h"

#include "condorCollector.nsmap"

#include "../condor_c++_util/soap_helpers.cpp"

static int receive_query_soap(int command,struct soap *s,char *constraint,
	struct condor__ClassAdStructArray &ads)
{

	// check for authorization here

	// construct a query classad from the constraint
	ClassAd query_ad;
	query_ad.SetMyTypeName(QUERY_ADTYPE);
	query_ad.SetTargetTypeName(ANY_ADTYPE);
	MyString req = ATTR_REQUIREMENTS;
	req += " = ";
	if ( constraint && constraint[0] ) {
		req += constraint;
	} else {
		req += "True";
	}
	query_ad.Insert(req.Value());

	// Initial query handler
	AdTypes whichAds = CollectorDaemon::receive_query_public( command );

	// actually process the query
	List<ClassAd> adList;
	query_ad.fPrint(stderr);
	CollectorDaemon::process_query_public (whichAds, &query_ad, &adList);

	// and fill in our soap struct response
	if ( !convert_adlist_to_adStructArray(s,&adList,&ads) ) {
		dprintf(D_ALWAYS, "receive_query_soap: convert_adlist_to_adStructArray failed!\n");
	}
	return SOAP_OK;
}

int condor__queryStartdAds(struct soap *s,char *constraint,
	struct condor__ClassAdStructArray & ads)
{
	int command = QUERY_STARTD_ADS;
	return receive_query_soap(command,s,constraint,ads);
}

int condor__queryScheddAds(struct soap *s,char *constraint,
	struct condor__ClassAdStructArray & ads)
{
	int command = QUERY_SCHEDD_ADS;
	return receive_query_soap(command,s,constraint,ads);
}

int condor__queryMasterAds(struct soap *s,char *constraint,
	struct condor__ClassAdStructArray & ads)
{
	int command = QUERY_MASTER_ADS;
	return receive_query_soap(command,s,constraint,ads);
}

int condor__querySubmittorAds(struct soap *s,char *constraint,
	struct condor__ClassAdStructArray & ads)
{
	int command = QUERY_SUBMITTOR_ADS;
	return receive_query_soap(command,s,constraint,ads);
}

int condor__queryLicenseAds(struct soap *s,char *constraint,
	struct condor__ClassAdStructArray & ads)
{
	int command = QUERY_LICENSE_ADS;
	return receive_query_soap(command,s,constraint,ads);
}

int condor__queryStorageAds(struct soap *s,char *constraint,
	struct condor__ClassAdStructArray & ads)
{
	int command = QUERY_STORAGE_ADS;
	return receive_query_soap(command,s,constraint,ads);
}

int condor__queryAnyAds(struct soap *s,char *constraint,
	struct condor__ClassAdStructArray & ads)
{
	int command = QUERY_ANY_ADS;
	return receive_query_soap(command,s,constraint,ads);
}


// TODO : This should move into daemonCore once we figure out how we wanna link

int condor__getPlatformString(struct soap *soap,void *,char* &result)
{
	const char *platform = CondorPlatform();
	result = (char *) soap_malloc(soap, strlen(platform) + 1);
	ASSERT(result);
	strcpy(result, platform);
	return SOAP_OK;
}

int condor__getVersionString(struct soap *soap,void *,char* &result)
{
	const char *version = CondorVersion();
	result = (char *) soap_malloc(soap, strlen(version) + 1);
	ASSERT(result);
	strcpy(result, version);
	return SOAP_OK;
}

int condor__getInfoAd(struct soap *soap,void *,struct condor__ClassAdStruct & ad)
{
	char* todd = "Todd A Tannenbaum";

	ad.__size = 3;
	ad.__ptr = (struct condor__ClassAdStructAttr *)soap_malloc(soap,3 * sizeof(struct condor__ClassAdStructAttr));

	ad.__ptr[0].name = "Name";
	ad.__ptr[0].type = STRING_ATTR;
	ad.__ptr[0].value = todd;

	ad.__ptr[1].name = "Age";
	ad.__ptr[1].type = INTEGER_ATTR;
	ad.__ptr[1].value = "35";
	int* age = (int*)soap_malloc(soap,sizeof(int));
	*age = 35;

	ad.__ptr[2].name = "Friend";
	ad.__ptr[2].type = STRING_ATTR;
	ad.__ptr[2].value = todd;

	return SOAP_OK;

}

///////////////////////////////////////////////////////////////////////////////
// TODO : This should move into daemonCore once we figure out how we wanna link
///////////////////////////////////////////////////////////////////////////////

#include "../condor_daemon_core.V6/soap_daemon_core.cpp"
