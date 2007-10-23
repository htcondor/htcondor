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

#include "condor_common.h"
#include "condor_debug.h"
#include "gid_pool.h"

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
