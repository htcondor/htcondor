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

#ifndef _JOBLOGREADER_H_
#define _JOBLOGREADER_H_

#include "../condor_quill/classadlogentry.h"
#include "../condor_quill/classadlogparser.h"
#include "../condor_quill/prober.h"

#define WANT_CLASSAD_NAMESPACE
#undef open
#include "classad/classad_distribution.h"

class JobLogReader {
public:
	JobLogReader();
	void poll(classad::ClassAdCollection *ad_collection);
	void SetJobLogFileName(char const *fname);
	char const *GetJobLogFileName();
private:
	Prober prober;
	ClassAdLogParser parser;

	bool BulkLoad(classad::ClassAdCollection *ad_collection);
	bool IncrementalLoad(classad::ClassAdCollection *ad_collection);
	bool ProcessLogEntry( ClassAdLogEntry *log_entry, classad::ClassAdCollection *ad_collection, ClassAdLogParser *caLogParser );

};

#endif
