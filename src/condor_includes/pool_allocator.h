/***************************************************************
 *
 * Copyright (C) 1013, John Knoeller, Condor Team, Computer Sciences Department,
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

#ifndef _POOL_ALLOCATOR
#define _POOL_ALLOCATOR

// depends on these headers
//#include "condor_header_features.h"

typedef struct _allocation_hunk {
	int ixFree;   // index of first free byte in pb
	int cbAlloc;  // allocation size of pb
	char * pb;
#ifdef __cplusplus
	_allocation_hunk() : ixFree(0), cbAlloc(0), pb(NULL) {};
	void reserve(int cb);
#endif
} ALLOC_HUNK;

typedef struct _allocation_pool {
	int nHunk;  // index of highest hunk that has free space
	int cMaxHunks; // number of hunks allocated
	ALLOC_HUNK * phunks; // this can vary in size.

#ifdef __cplusplus
	_allocation_pool(int cMax=0) : nHunk(0), cMaxHunks(cMax), phunks(NULL) {
		if (cMaxHunks) {
			phunks = new ALLOC_HUNK[cMax];
			if ( ! phunks) cMax = 0;
		}
	};
	int alignment() const { return 1; };
	int def_first_alloc() const { return 4 * 1024; };
	void clear();
	char * consume(int cb, int cbAlign);
	const char * insert(const char * pbInsert, int cbInsert);
	const char * insert(const char * psz);
	bool contains(const char * pb);
	void free(const char * psz);
	void reserve(int cbLeaveFree);
	void compact(int cbLeaveFree);
	void swap(struct _allocation_pool & other);
	int  usage(int & cHunks, int & cbFree); // returns total used size.
#endif

} ALLOCATION_POOL;


// c++ implementation and interface here.
#ifdef __cplusplus

#endif

// c callable public interface begins here.
BEGIN_C_DECLS

/*
void CreateAllocationPool(ALLOC_POOL * apool);
const char * AllocInPool(ALLOC_POOL * apool, const char * psz);
void ClearAllocationPool(ALLOC_POOL * apool);
*/

END_C_DECLS


#endif /* _POOL_ALLOCATOR */
