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
#include "condor_debug.h"
#include "classad_newold.h"

#include "JobLogReader.h"

#include <string>

#define WANT_CLASSAD_NAMESPACE
#undef open
#include "classad/classad_distribution.h"

class NewClassAdJobLogConsumer: public JobLogConsumer
{
private:

	classad::ClassAdCollection m_collection;
	JobLogReader *m_reader;

public:

	NewClassAdJobLogConsumer();

	classad::ClassAdCollection *GetClassAds() {return &m_collection;}

	void Reset();

	bool NewClassAd(const char *key,
					const char */*type*/,
					const char */*target*/);

	bool DestroyClassAd(const char *key);

	bool SetAttribute(const char *key,
					  const char *name,
					  const char *value);

	bool DeleteAttribute(const char *key,
						 const char *name);

	void SetJobLogReader(JobLogReader *_reader) { m_reader = _reader; }
};
