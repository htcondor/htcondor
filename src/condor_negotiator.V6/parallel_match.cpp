/***************************************************************
 *
 * Copyright (C) 1990-2026, Condor Team, Computer Sciences Department,
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
#include "condor_classad.h"
#include "parallel_match.h"

#ifdef _OPENMP
#include <omp.h>
#endif

static classad::MatchClassAd *match_pool = nullptr;
static ClassAd *target_pool = nullptr;
static std::vector<ClassAd*> *matched_ads = nullptr;

bool
ParallelIsAMatch(ClassAd *ad1, std::vector<ClassAd*> &candidates, std::vector<ClassAd*> &matches, int threads, bool halfMatch)
{
	int adCount = candidates.size();
	static size_t cpu_count = 0;
	size_t current_cpu_count = threads;
	int iterations = 0;
	size_t matched = 0;

	if(cpu_count != current_cpu_count)
	{
		cpu_count = current_cpu_count;
		if(match_pool)
		{
			delete[] match_pool;
			match_pool = nullptr;
		}
		if(target_pool)
		{
			delete[] target_pool;
			target_pool = nullptr;
		}
		if(matched_ads)
		{
			delete[] matched_ads;
			matched_ads = nullptr;
		}
	}

	if(!match_pool)
		match_pool = new classad::MatchClassAd[cpu_count];
	if(!target_pool)
		target_pool = new ClassAd[cpu_count];
	if(!matched_ads)
		matched_ads = new std::vector<ClassAd*>[cpu_count];

	if(!candidates.size())
		return false;

	for(size_t index = 0; index < cpu_count; index++)
	{
		target_pool[index].CopyFrom(*ad1);
		match_pool[index].ReplaceLeftAd(&(target_pool[index]));
		matched_ads[index].clear();
	}

	iterations = ((candidates.size() - 1) / cpu_count) + 1;

#ifdef _OPENMP
	omp_set_num_threads(cpu_count);
#endif

#pragma omp parallel
	{

#ifdef _OPENMP
		int omp_id = omp_get_thread_num();
#else
		int omp_id = 0;
#endif
		for(int index = 0; index < iterations; index++)
		{
			bool result = false;
			int offset = omp_id + index * cpu_count;
			if(offset >= adCount)
				break;
			ClassAd *ad2 = candidates[offset];

			match_pool[omp_id].ReplaceRightAd(ad2);
		
			if(halfMatch)
				result = match_pool[omp_id].rightMatchesLeft();
			else
				result = match_pool[omp_id].symmetricMatch();

			match_pool[omp_id].RemoveRightAd();

			if(result)
			{
				matched_ads[omp_id].push_back(ad2);
			}
		}
	}

	for(size_t index = 0; index < cpu_count; index++)
	{
		match_pool[index].RemoveLeftAd();
		matched += matched_ads[index].size();
	}

	if(matches.capacity() < matched)
		matches.reserve(matched);

	for(size_t index = 0; index < cpu_count; index++)
	{
		if(matched_ads[index].size())
			matches.insert(matches.end(), matched_ads[index].begin(), matched_ads[index].end());
	}

	return matches.size() > 0;
}
