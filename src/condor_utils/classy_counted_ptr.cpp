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
#include "classy_counted_ptr.h"

// These are defined out-of-line on purpose; see the comment in
// classy_counted_ptr.h.  Keeping the virtual destructor and decRefCount
// out-of-line gives the vtable a stable key function and prevents the
// `delete this` below from being inlined+devirtualized at every call
// site (which on derived classes that place ClassyCountedPtr at a
// nonzero subobject offset triggers a -Wfree-nonheap-object warning
// under GCC 16).

ClassyCountedPtr::~ClassyCountedPtr()
{
	ASSERT( m_ref_count == 0 );
}

void
ClassyCountedPtr::decRefCount()
{
	ASSERT( m_ref_count > 0 );
	if( --m_ref_count == 0 ) {
		delete this;
	}
}
