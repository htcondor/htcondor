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
#include "gid_pool.linux.h"

GIDPool::GIDPool(gid_t min_gid, gid_t max_gid)
{
	ASSERT(min_gid <= max_gid);
	m_size = max_gid - min_gid + 1;
	m_offset = min_gid;
	m_gid_map = new ProcFamily*[m_size];
	ASSERT(m_gid_map != NULL);
	for (int i = 0; i < m_size; i++) {
		m_gid_map[i] = NULL;
	}
}

GIDPool::~GIDPool()
{
	delete[] m_gid_map;
}

bool
GIDPool::allocate(ProcFamily* family, gid_t& gid)
{
	for (int i = 0; i < m_size; i++) {
		if (m_gid_map[i] == NULL) {
			m_gid_map[i] = family;
			gid = m_offset + i;
			return true;
		}
	}
	return false;
}

bool
GIDPool::free(ProcFamily* family)
{
	for (int i = 0; i < m_size; i++) {
		if (m_gid_map[i] == family) {
			m_gid_map[i] = NULL;
			return true;
		}
	}
	return false;
}

ProcFamily*
GIDPool::get_family(gid_t gid)
{
	int index = gid - m_offset;
	if ((index < 0) || (index >= m_size)) {
		return NULL;
	}
	return m_gid_map[index];
}
