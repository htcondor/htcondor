/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
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

	// actually process the query
	List<ClassAd> adList;
	int result = CollectorDaemon::receive_query_public(command,&query_ad,&adList);

	// and fill in our soap struct response
	if ( !convert_adlist_to_adStructArray(s,&adList,&ads) ) {
		dprintf(D_ALWAYS,"receive_query_soap: convert_adlist_to_adStructArray failed!\n");
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
	result = CondorPlatform();
	return SOAP_OK;
}

int condor__getVersionString(struct soap *soap,void *,char* &result)
{
	result = CondorVersion();
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
