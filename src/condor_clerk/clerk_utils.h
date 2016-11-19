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

#ifndef _CLERK_UTILS_H_
#define _CLERK_UTILS_H_

#include "dc_collector.h"
#include "compat_classad_util.h"

// this holds variables used by the clerk, that are not params
extern MACRO_SET ClerkVars;
extern MACRO_EVAL_CONTEXT_EX ClerkEvalCtx;

template<typename T> class CollectionIterator
{
public:
	virtual bool IsEmpty() const = 0;
	virtual void Rewind() = 0;
	virtual T Next () = 0;
	virtual T Current() const = 0;
	virtual bool AtEnd() const = 0;
	virtual ~CollectionIterator() {};
};


void remove_self_from_collector_list(CollectorList * colist, bool ImTheCollector);

void update_ads_from_file(int operation, AdTypes adtype, const char * file, CondorClassAdFileParseHelper::ParseType ftype, ConstraintHolder & constr);

CollectionIterator<ClassAd*>* clerk_get_file_iter(
	const char * file,
	bool is_command,
	char * constraint=NULL,
	CondorClassAdFileParseHelper::ParseType ftype = CondorClassAdFileParseHelper::Parse_auto
);
void collect_free_file_iter(CollectionIterator<ClassAd*> * it);

typedef std::map<std::string, std::string, classad::CaseIgnLTStr> NOCASE_STRING_MAP;
bool param_and_populate_map(const char * param_name, NOCASE_STRING_MAP & smap);

const char * AdTypeName(AdTypes whichAds);
AdTypes AdTypeByName(const char * name);

#endif
