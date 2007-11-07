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

//gsoap condor service name: condorCollector

#import "gsoap_daemon_core_types.h"

#import "gsoap_daemon_core.h"

enum condor__ClassAdType
{
	STARTD_AD_TYPE,
	QUILL_AD_TYPE,
	SCHEDD_AD_TYPE,
	SUBMITTOR_AD_TYPE,
	LICENSE_AD_TYPE,
	MASTER_AD_TYPE,
	CKPTSRVR_AD_TYPE,
	COLLECTOR_AD_TYPE,
	STORAGE_AD_TYPE,
	NEGOTIATOR_AD_TYPE,
	HAD_AD_TYPE,
	GENERIC_AD_TYPE
};

int condor__insertAd(enum condor__ClassAdType type,
					 struct condor__ClassAdStruct ad,
					 struct condor__insertAdResponse {
						 struct condor__Status status;
					 } & result);

int condor__queryStartdAds(char *constraint,
						   struct condor__ClassAdStructArray & result);

int condor__queryScheddAds(char *constraint,
						   struct condor__ClassAdStructArray & result);

int condor__queryMasterAds(char *constraint,
						   struct condor__ClassAdStructArray & result);

int condor__querySubmittorAds(char *constraint,
							  struct condor__ClassAdStructArray & result);

int condor__queryLicenseAds(char *constraint,
							struct condor__ClassAdStructArray & result);

int condor__queryStorageAds(char *constraint,
							struct condor__ClassAdStructArray & result);

int condor__queryAnyAds(char *constraint,
						struct condor__ClassAdStructArray & result);

