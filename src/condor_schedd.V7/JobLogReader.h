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
#ifndef _JOBLOGREADER_H_
#define _JOBLOGREADER_H_

#include "../condor_quill/classadlogentry.h"
#include "../condor_quill/classadlogparser.h"
#include "../condor_quill/prober.h"

#define WANT_NAMESPACES
#include "classad_distribution.h"

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
