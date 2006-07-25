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

