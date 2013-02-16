/***************************************************************
 *
 * Copyright (C) 2009-2011 Red Hat, Inc.
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

#ifndef _JOBSERVERJOBLOGCONSUMER_H
#define _JOBSERVERJOBLOGCONSUMER_H

// condor includes
#include "condor_common.h"
#include "condor_debug.h"
#include "ClassAdLogReader.h"

// local includes
#include "Job.h"
#include "SubmissionObject.h"

// c++ includes
#include <string>
#include <map>
#include <set>

using namespace std;

class JobServerJobLogConsumer: public ClassAdLogConsumer
{
public:
	JobServerJobLogConsumer();
	~JobServerJobLogConsumer();

	void Reset();
	bool NewClassAd(const char *key,
					const char *type,
					const char *target);
	bool DestroyClassAd(const char *key);
	bool SetAttribute(const char *key,
					  const char *name,
					  const char *value);
	bool DeleteAttribute(const char *key,
						 const char *name);
    void SetJobLogReader(ClassAdLogReader *_reader) { m_reader = _reader; }


private:

    ClassAdLogReader *m_reader;
};

#endif /* _JOBSERVERJOBLOGCONSUMER_H */
