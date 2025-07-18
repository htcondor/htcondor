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

#ifndef _JOB_AD_INSTANCE_RECORDING_H
#define _JOB_AD_INSTANCE_RECORDING_H

#include "condor_classad.h"

//Macros for Epoch dir filename formats
#define EPOCH_PER_JOB_FILE "job.runs.%d.%d.ads"
//TODO: Add filename format for per cluster option if implemented

// Write Full ClassAd to Epoch history.
// Optionally, name historical ad type in banner
void writeAdToEpoch(
	const classad::ClassAd *ad,
	const char* banner_name = "EPOCH"
);

// Write filtered ClassAd to Epoch history.
// Optionally, name historical ad type in banner
void writeAdProjectionToEpoch(
	const classad::ClassAd *ad,
	const classad::References* filter,
	const char* banner_name = "EPOCH"
);

// Write ClassAd with attributes copied from job ad (context)
// List of copied attributes come from configuration
// derived by historical ad type in banner
void writeAdWithContextToEpoch(
	const classad::ClassAd *ad,
	const classad::ClassAd *job_ad,
	const char* banner_name
);

#endif
