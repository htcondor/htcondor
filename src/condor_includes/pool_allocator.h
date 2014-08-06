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

/*
 * This is a simple pool allocator for storing string data, it is intended 
 * to be used in conjunction with other data structures that that want to
 * mix pointers to string literals with pointers to dynamic strings.
 *
 * strings stored in this pool can be treated as string literals in other
 * data structures.  They can be freed all at once when no longer needed.
 * They cannot be freed individually. So use the pool allocator only in
 * conjunction with other data structures that have finite lifetime.
 *
 * For instance, if you have List<const char> that you want to use to store
 * data temporarily, you can do this.
 *
 *    POOL_ALLOCATOR pool;
 *    List<const char> stuff;

 *    // build the list
 *    stuff.insert("literal"); // literals can be stored directly in in the list
 *    MyString number; number.formatstr("%d", num);
 *    const char * number_lit = pool.insert(number.Value()); // generated strings are stored in the string pool
 *    stuff.insert(number_lit); // and can then be treated as a string literal in the List
 *
 *    // use the list here
 *
 *    // free the list and pool of strings (if any)
 *    stuff.Rewind(); while (stuff.DeleteCurrent());
 *    pool.clear();
 *
 */

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
	// free all pool memory
	void clear();
	// allocate pool memory and return a pointer 
	char * consume(int cb, int cbAlign);
	// insert arbitrary data into the pool and return a pointer.
	const char * insert(const char * pbInsert, int cbInsert);
	// insert a string into the pool and return a pointer.
	const char * insert(const char * psz);
	// check to see if a pointer was allocated from this pool
	bool contains(const char * pb);
	// free an allocation and everything allocated after it.
	// may fail if pb is not the most recent allocation.
	void free(const char * pb);
	// reserve space in the pool. (allocates at least this much contiguous from the system)
	void reserve(int cbLeaveFree);
	// compact the pool, leaving at least this much free space
	void compact(int cbLeaveFree);
	// swap allocations with another pool. (used to defragment the param table)
	void swap(struct _allocation_pool & other);
	// get total used size, total number of system allocations and amount of free space in the pool.
	int  usage(int & cHunks, int & cbFree);
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
