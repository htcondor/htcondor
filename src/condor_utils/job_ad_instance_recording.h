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

// Function for Shadow to call to write Job Ad per Epoch
void writeJobEpochFile(
	classad::ClassAd const *job_ad,
	classad::ClassAd const *other_ad = NULL,
	const char *banner_name = "EPOCH"
);

#endif
