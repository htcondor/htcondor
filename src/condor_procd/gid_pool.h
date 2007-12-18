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


#ifndef _IDENTIFIER_POOL_H
#define _IDENTIFIER_POOL_H

#include <sys/types.h>

class ProcFamily;

class GIDPool {

public:
	GIDPool(gid_t min_gid, gid_t max_gid);
	~GIDPool();
	bool allocate(ProcFamily* family, gid_t& gid);
	bool free(ProcFamily* family);
	ProcFamily* get_family(gid_t gid);

private:
	int   m_size;
	gid_t m_offset;
	ProcFamily** m_gid_map;
};

#endif
