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

// Things to include for the stubs
#include "condor_version.h"
#include "collector.h"
#include "condor_attributes.h"

#include "condorCollector.nsmap"

using namespace soap_collector;
#include "../condor_utils/soap_helpers.cpp"

extern CollectorDaemon* Daemon;

//struct timeval convert_start_time, convert_end_time, convert_time_diff;

namespace soap_collector {

int
condor__insertAd(struct soap *soap,
				 enum condor__ClassAdType type,
				 struct condor__ClassAdStruct adStruct,
				 struct condor__insertAdResponse &result)
{
	dprintf(D_ALWAYS, "Entering condor__insertAd...\n");

		// Instead of a big switch an array index could be used, but
		// it would mean keeping the order of things in the
		// condor__ClassAdType enum and this code in sync.
	int command;
	switch (type) {
	case STARTD_AD_TYPE:     command = UPDATE_STARTD_AD;     break;
	case QUILL_AD_TYPE:      command = UPDATE_QUILL_AD;      break;
	case SCHEDD_AD_TYPE:     command = UPDATE_SCHEDD_AD;     break;
	case SUBMITTOR_AD_TYPE:  command = UPDATE_SUBMITTOR_AD;  break;
	case LICENSE_AD_TYPE:    command = UPDATE_LICENSE_AD;    break;
	case MASTER_AD_TYPE:     command = UPDATE_MASTER_AD;     break;
	case CKPTSRVR_AD_TYPE:   command = UPDATE_CKPT_SRVR_AD;  break;
	case COLLECTOR_AD_TYPE:  command = UPDATE_COLLECTOR_AD;  break;
	case STORAGE_AD_TYPE:    command = UPDATE_STORAGE_AD;    break;
	case NEGOTIATOR_AD_TYPE: command = UPDATE_NEGOTIATOR_AD; break;
	case HAD_AD_TYPE:        command = UPDATE_HAD_AD;        break;
	case GENERIC_AD_TYPE:    command = UPDATE_AD_GENERIC;    break;
	default:
		command = -1; // Let's hope a command is never -1
		result.status.code = FAIL;
		result.status.message = "Unknown type";
		return SOAP_OK;
		break;
	}

	ClassAd *ad = new ClassAd;
	condor_sockaddr from;				// We don't need this ONLY because
								// it is ignored in all hash
								// functions in hashkey.C
	int insert;	// Return value from CollectorEngine::collect
	Sock *sock = NULL; // Updating of Startd ads will fail without
					   // this. It is needed to retrieve the
					   // startd private ad.
	if (!ad) {
		result.status.code = FAIL;
		result.status.message = "Collestor has run out of memory, sorry";
		return SOAP_OK;
	}

	if (!convert_adStruct_to_ad(soap, ad, &adStruct)) {
		dprintf(D_FULLDEBUG,
				"Failed to convert ClassAdStruct to ClassAd.\n");

		result.status.code = FAIL;
		result.status.message = "Failed to parse the ClassAdStruct";
		delete ad;
		return SOAP_OK;
	}

	if (!Daemon->getCollector().collect(command,
										ad,
										from,
										insert,
										sock)) {
		result.status.code = FAIL;
		result.status.message = "Failed to parse the ClassAdStruct";
		delete ad;
		return SOAP_OK;
	}

		// Finally, do some real work.
	switch (insert) {
	case 0:
		result.status.code = SUCCESS;
		result.status.message = "Updated ad";
		break;
	case 1:
		result.status.code = SUCCESS;
		result.status.message = "New ad inserted";
		break;
	case -3:
		result.status.code = FAIL;
		result.status.message = "Internal update failure";
		break;
	case -2:
		result.status.code = FAIL;
		result.status.message = "Unsupported internal command";
		break;
	case -1:
		result.status.code = FAIL;
		result.status.message = "Illegal internal command";
		break;
	default:
		result.status.code = FAIL;
		result.status.message = "Unknown internal error code";
		break;
	}

		// Don't leak a ClassAd on error, otherwise it becomes the
		// CollectorEngine's responsibility
	if (FAIL == result.status.code) {
		if (ad) {
			delete ad;
			ad = NULL;
		}
	}

	dprintf(D_ALWAYS, "Leaving condor__insertAd...\n");

	return SOAP_OK;
}

}

static int receive_query_soap(int command,struct soap *s,char *constraint,
	struct condor__ClassAdStructArray &ads)
{

	// check for authorization here

	// construct a query classad from the constraint
	ClassAd query_ad;
	SetMyTypeName(query_ad, QUERY_ADTYPE);
	SetTargetTypeName(query_ad, ANY_ADTYPE);
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
	fPrintAd(stderr, query_ad);
	CollectorDaemon::process_query_public (whichAds, &query_ad, &adList);

//	ASSERT(0 == gettimeofday(&convert_start_time, NULL));
	// and fill in our soap struct response
	if ( !convert_adlist_to_adStructArray(s,&adList,&ads) ) {
		dprintf(D_ALWAYS, "receive_query_soap: convert_adlist_to_adStructArray failed!\n");
	}
//	ASSERT(0 == gettimeofday(&convert_end_time, NULL));
//	timersub(&convert_end_time, &convert_start_time, &convert_time_diff);

//	fprintf(stdout,
//			"TIMING: CONVERT: %ld.%.6ld\n",
//			convert_time_diff.tv_sec, convert_time_diff.tv_usec);

	return SOAP_OK;
}

namespace condor_soap {

int
soap_serve(struct soap *soap)
{
        return soap_collector::soap_serve(soap);
}

}

namespace soap_collector {

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

///////////////////////////////////////////////////////////////////////////////
// TODO : This should move into daemonCore once we figure out how we wanna link
///////////////////////////////////////////////////////////////////////////////

#include "soap_daemon_core.cpp"
}
