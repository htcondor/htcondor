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
//gsoap condor service style: document
//gsoap condor service encoding: literal
//gsoap condor service namespace: urn:condor

#import "gsoap_daemon_core_types.h"

#import "gsoap_daemon_core.h"

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


