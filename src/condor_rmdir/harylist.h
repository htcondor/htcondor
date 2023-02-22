/***************************************************************
 *
 * Copyright (C) 2010, John M. Knoeller
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

#pragma once

#ifndef FNEXPORT
#define FNEXPORT _declspec(dllexport) CALLBACK
#endif

#ifndef INLINE
#define INLINE __inline
#endif

struct tHARYLIST;
typedef tHARYLIST* HARYLIST;
typedef HARYLIST *PHARYLIST;
typedef const HARYLIST *PCHARYLIST;

#define ARYLIST_OPT_F_INITIALIZED 0x0001 // list header has been initialized
#define ARYLIST_OPT_F_SERIALIZE   0x0010 // list contains a critical section
#define ARYLIST_OPT_F_EMBEDDED    0x0020 // list header is embedded in another structure.
#define ARYLIST_OPT_F_GPTRS       0x0040 // list is an array of allocated pointers

template<class T>
struct HaryList
{
    DWORD            fdwOptions;  // one or more of ARYLIST_OPT_F_xxx flags
    LONG             cItems;      // number of elements in pvList that contain data
    LONG             cAllocated;  // number of elements in pvList that are allocated.
    LONG             cbItem;      // size of each element in pvList
    LONG             cGrowBy;
    LONG             user;        // for use by clients of this code. also aligns pvList
    T*               pvList;
    CRITICAL_SECTION cs;
};

// the generic list/vector of pointers to items
// is a specialization of the HaryList structure
// for void* as the item type.  
typedef HaryList<void*> ARYLIST;
typedef ARYLIST *PARYLIST;
typedef const ARYLIST *PCARYLIST;

#define PCARYLIST_PTR(hlst) ((PCARYLIST)(hlst))

#define HaryList_Lock(hlst)   EnterCriticalSection(&PCARYLIST_PTR(hlst)->cs)
#define HaryList_Unlock(hlst) LeaveCriticalSection(&PCARYLIST_PTR(hlst)->cs)

#define HaryList_GetCount(hlst)      (PCARYLIST_PTR(hlst)->cItems)
#define HaryList_GetCountSafe(hlst)  (( ! hlst) ? 0 : HaryList_GetCount(hlst))

HRESULT FNEXPORT HaryList_Create (
    PHARYLIST phlst,
    LONG      cbItem,
    LONG      cAllocate,
    LONG      cGrowBy,
	DWORD     fdwOptions);

INLINE HRESULT HaryList_CreateIfNot (
    PHARYLIST phlst,
    LONG      cbItem,
    LONG      cAllocate,
    LONG      cGrowBy,
	DWORD     fdwOptions)
{
	if (*phlst && PCARYLIST_PTR(*phlst)->pvList)
		return S_FALSE;
	return HaryList_Create(phlst, cbItem, cAllocate, cGrowBy, fdwOptions);
}

INLINE HRESULT HaryList_Create (
	PARYLIST  plst,
    LONG      cbItem, 
	LONG      cAllocate, 
	LONG      cGrowBy, 
	DWORD     fdwOptions) 
{
	return HaryList_Create((PHARYLIST)plst, cbItem, cAllocate, cGrowBy, fdwOptions | ARYLIST_OPT_F_EMBEDDED);
}

INLINE HRESULT HaryList_CreateIfNot (
	PARYLIST  plst,
    LONG      cbItem, 
	LONG      cAllocate, 
	LONG      cGrowBy, 
	DWORD     fdwOptions) 
{
	if (plst->pvList)
		return S_FALSE;
	return HaryList_Create((PHARYLIST)plst, cbItem, cAllocate, cGrowBy, fdwOptions | ARYLIST_OPT_F_EMBEDDED);
}

HRESULT FNEXPORT HaryList_Destroy (
    HARYLIST  hlst);

HRESULT FNEXPORT HaryList_InsertList (
    HARYLIST  hlst,
    LONG      ixInsert,
    LPCVOID   pvItems,
    LONG      cItemsToInsert,
	LONG *    pixLoc = NULL);

INLINE LPVOID HaryList_AllocItem(HARYLIST hlst, LONG cbItem) {
	DASSERT(PCARYLIST_PTR(hlst)->cbItem == sizeof(void*));
	DASSERT(PCARYLIST_PTR(hlst)->fdwOptions & ARYLIST_OPT_F_GPTRS);
	LPVOID pv = (LPVOID)malloc(cbItem);
    if (pv)
       ZeroMemory(pv, cbItem);
    return pv;
}

INLINE void HaryList_FreeItem(HARYLIST hlst, LPVOID pvItem) {
	DASSERT(PCARYLIST_PTR(hlst)->cbItem == sizeof(void*));
	DASSERT(PCARYLIST_PTR(hlst)->fdwOptions & ARYLIST_OPT_F_GPTRS);
	free(pvItem);
}

INLINE HRESULT HaryList_InsertItemCopy(HARYLIST hlst, LONG ix, LPCVOID pvItem) {
    return HaryList_InsertList(hlst, ix, pvItem, 1);
}
INLINE LPVOID HaryList_RawItemAddr(PARYLIST plst, LONG ix) {
    return (LPVOID)((LPBYTE)plst->pvList + (ix * plst->cbItem));
}
INLINE LPVOID HaryList_RawItemAddr(HARYLIST hlst, LONG ix) {
    return HaryList_RawItemAddr((PARYLIST)hlst, ix);
}
INLINE LPARAM HaryList_RawItemCopy(PARYLIST plst, LONG ix) {
	if (plst->cbItem == sizeof(LPARAM))
		return *(LPARAM*)HaryList_RawItemAddr(plst, ix);
   #ifdef _WIN64
    else if (plst->cbItem == sizeof(int))
		return *(int*)HaryList_RawItemAddr(plst, ix);
   #endif
    return 0;
}
INLINE LPARAM HaryList_RawItemCopy(HARYLIST hlst, LONG ix) {
    return HaryList_RawItemCopy((PARYLIST)hlst, ix);
}

template <class T>
INLINE T HaryList_GetItem(HARYLIST hlst, LONG ix) {
	return (T)HaryList_RawItemCopy(hlst, ix);
}

template <class T>
HRESULT HaryList_Iterate (HARYLIST hlst, HRESULT (CALLBACK *pfn)(T, LPARAM), LPARAM lParam)
{
   HRESULT  hr = S_FALSE;
   LONG cFiles = HaryList_GetCountSafe(hlst);

   for (LONG ii = 0; ii < cFiles; ii++)
      {
      T pItem = HaryList_GetItem<T>(hlst, ii);
      hr = pfn(pItem, lParam);
	  if (FAILED(hr))
		  break;
      }

   return hr;
}

// templatized wrappers around the HaryList_xxx functions so that they
// we can make type specific lists..
//
#if 1
template <class T>
HRESULT Vector_Create(HaryList<T> ** phlst, LONG cItems, LONG cGrowBy = -1, DWORD fdwOpts = 0) {
   return HaryList_Create ((PHARYLIST)phlst, sizeof(T), cItems, cGrowBy, fdwOpts);
}

template <class T>
HRESULT Vector_CreateIfNot(HaryList<T> ** phlst, LONG cItems, LONG cGrowBy = -1, DWORD fdwOpts = 0) {
   return HaryList_CreateIfNot ((PHARYLIST)phlst, sizeof(T), cItems, cGrowBy, fdwOpts);
}
template <class T>
HRESULT Vector_AppendItem(HaryList<T> * hlst, const T & item) {
   return HaryList_InsertItemCopy ((HARYLIST)hlst, -1, &item);
}

template <class T>
HRESULT Vector_InsertItem(HaryList<T> * hlst, LONG ix, const T & item) {
   return HaryList_InsertItemCopy ((HARYLIST)hlst, ix, &item);
}

template <class T>
T* Vector_GetItemPtr(HaryList<T> * hlst, LONG ix) {
	return (T*)HaryList_RawItemAddr ((HARYLIST)hlst, ix);
}

template <class T>
T* Vector_GetItem(HaryList<T> * hlst, LONG ix) {
	return *(T*)HaryList_RawItemAddr ((HARYLIST)hlst, ix);
}

template <class T>
INLINE HRESULT Vector_Delete(HaryList<T> * hlst) {
	return HaryList_Destroy ((HARYLIST)hlst); 
}

template <class T>
INLINE LONG Vector_GetCountSafe(HaryList<T> * hlst) { 
	return HaryList_GetCountSafe((HARYLIST)hlst); 
}

template <class T>
INLINE LONG Vector_GetCount(HaryList<T> * hlst) { 
	return HaryList_GetCount((HARYLIST)hlst); 
}

#else
/*
template <class T>
HRESULT Vector_Create(HARYLIST * phlst, LONG cItems, LONG cGrowBy = -1) {
   return HaryList_Create (phlst, sizeof(T), cItems, cGrowBy, 0);
}

template <class T>
HRESULT Vector_AppendItem(HARYLIST hlst, const T & item) {
   return HaryList_InsertItemCopy (hlst, -1, &item);
}

template <class T>
HRESULT Vector_InsertItem(HARYLIST hlst, LONG ix, const T & item) {
   return HaryList_InsertItemCopy (hlst, ix, &item);
}

template <class T>
T* Vector_GetItemPtr(HARYLIST hlst, LONG ix) {
	return (T*)HaryList_RawItemAddr (hlst, ix);
}

template <class T>
T* Vector_GetItem(HARYLIST hlst, LONG ix) {
	return *(T*)HaryList_RawItemAddr (hlst, ix);
}

INLINE HRESULT Vector_Delete(HARYLIST hlst)    { return HaryList_Destroy (hlst); }
INLINE LONG Vector_GetCountSafe(HARYLIST hlst) { return HaryList_GetCountSafe(hlst); }
INLINE LONG Vector_GetCount(HARYLIST hlst)     { return HaryList_GetCount(hlst); }
*/
#endif

//
// templatized wrappers for using HaryList as an array of pointers to a 
// specific type. Just declare your HaryList HANDLE as
//
//    typedef HaryList<my_pointer_type> * HMYTYPE_LIST;
//
// and then declare the handle variable as 
//
//    HMYTYPE_LIST hlst;
//
// then when you call the HaryList_xxx functions, the type of the hlst will force
// the functions to spcialize and return or accept my_pointer_type items.
//
template <class T>
HRESULT PtrList_Create(HaryList<T> ** phlst, LONG cItems, LONG cGrowBy = -1, DWORD fdwOpts = 0) {
   return HaryList_Create ((PHARYLIST)phlst, sizeof(T), cItems, cGrowBy, fdwOpts | ARYLIST_OPT_F_GPTRS);
}

template <class T>
HRESULT PtrList_CreateIfNot(HaryList<T> ** phlst, LONG cItems, LONG cGrowBy = -1, DWORD fdwOpts = 0) {
   return HaryList_CreateIfNot ((PHARYLIST)phlst, sizeof(T), cItems, cGrowBy, fdwOpts | ARYLIST_OPT_F_GPTRS);
}

template <class T>
T PtrList_AllocItem(HaryList<T> * hlst, LONG cbItem = sizeof(T)) {
   DASSERT(cbItem >= sizeof(*T));
   return static_cast<T>(HaryList_AllocItem ((HARYLIST)hlst, cbItem));
}

template <class T>
HRESULT PtrList_AppendItem(HaryList<T> * hlst, T pItem, LONG * pix = NULL) {
   DASSERT(sizeof(T) == sizeof(void*));
   return HaryList_InsertList((HARYLIST)hlst, -1, &pItem, 1, pix);
}

template <class T>
HRESULT PtrList_InsertItem(HaryList<T> * hlst, LONG ixInsert, T pItem) {
   return HaryList_InsertList((HARYLIST)hlst, ixInsert, &pItem, 1);
}

template <class T>
INLINE LONG PtrList_GetCountSafe(HaryList<T> * hlst) { 
	return HaryList_GetCountSafe((HARYLIST)hlst); 
}

template <class T>
INLINE T PtrList_GetItem(HaryList<T> * hlst, LONG ix) {
	return HaryList_GetItem<T>((HARYLIST)hlst, ix);
}

template <class T>
INLINE HRESULT PtrList_Delete(HaryList<T> * hlst) { 
	return HaryList_Destroy((HARYLIST)hlst); 
}

template <class T>
HRESULT PtrList_Iterate (HaryList<T> * hlst, HRESULT (CALLBACK *pfn)(T, LPARAM), LPARAM lParam) {
	return HaryList_Iterate((HARYLIST)hlst, pfn, lParam);
}

#ifdef INCLUDE_HARYLIST_PTRVECTOR
//
// templatized wrappers for PtrVector, for using the HaryList as a container
// for an array of pointers to a single type of data.  (not fully type safe...)
//
template <class T>
HRESULT PtrVector_Create(HARYLIST * phlst, LONG cItems, LONG cGrowBy = -1) {
   return HaryList_Create (phlst, sizeof(T*), cItems, cGrowBy, ARYLIST_OPT_F_GPTRS);
}

template <class T>
HRESULT PtrVector_CreateIfNot(HARYLIST * phlst, LONG cItems, LONG cGrowBy = -1) {
   return HaryList_CreateIfNot (phlst, sizeof(T*), cItems, cGrowBy, ARYLIST_OPT_F_GPTRS);
}

template <class T>
T* PtrVector_AllocItem(HARYLIST hlst, LONG cbItem = sizeof(T)) {
   DASSERT(cbItem >= sizeof(T));
   return static_cast<T*>(HaryList_AllocItem (hlst, cbItem));
}

template <class T>
HRESULT PtrVector_InsertItem(HARYLIST hlst, T pItem, LONG * pix = NULL) {
   DASSERT(sizeof(T) == sizeof(void*));
   return HaryList_InsertList(hlst, -1, &pItem, 1, pix);
}

template <class T>
INLINE T PtrVector_GetItem(HARYLIST hlst, LONG ix) {
	return HaryList_GetItem<T>(hlst, ix);
}

INLINE LONG PtrVector_GetCountSafe(HARYLIST hlst) { return HaryList_GetCountSafe(hlst); }
INLINE LONG PtrVector_GetCount(HARYLIST hlst)     { return HaryList_GetCount(hlst); }
INLINE HRESULT PtrVector_Delete(HARYLIST hlst)    { return HaryList_Destroy(hlst); }

template <class T>
HRESULT PtrVector_Iterate (HARYLIST hlst, HRESULT (CALLBACK *pfn)(T, LPARAM), LPARAM lParam)
{
	return HaryList_Iterate((HARYLIST)hlst, pfn, lParam);
}
#endif INCLUDE_HARYLIST_PTRVECTOR

//
//
//

//
// The HaryList code is below this line. include it in only one module
//
#ifdef HARYLIST_INCLUDE_CODE

#define DivRU(x, y)     (((x) + ((y) - 1)) / (y))
#define DivRN(x, y)     (((x) + ((y) / 2)) / (y))
INLINE DWORD ALIGN(SIZE_T cb, SIZE_T cbAlign) { return (DWORD)( (cb + (cbAlign - 1)) & ~(cbAlign - 1) );}

HRESULT FNEXPORT HaryList_Create (
    PHARYLIST phlst,
    LONG      cbItem,
    LONG      cAllocate,
    LONG      cGrowBy,
	DWORD     fdwOptions)
{
	if ( ! phlst)
		return E_INVALIDARG;

    PARYLIST  plst = NULL;
	if (fdwOptions & ARYLIST_OPT_F_EMBEDDED)
		plst = (PARYLIST)phlst;
	else
	{
		*phlst = NULL;
		plst = (PARYLIST)malloc(sizeof(*plst));
        if (plst)
           ZeroMemory(plst, sizeof(*plst));
	}
	if ( ! plst)
		return E_OUTOFMEMORY;

    plst->fdwOptions = fdwOptions | ARYLIST_OPT_F_INITIALIZED;
    if (fdwOptions & ARYLIST_OPT_F_SERIALIZE) {
        #pragma warning(suppress: 28125) // warning InitCritSec should be called in a try/except block
        InitializeCriticalSection(&plst->cs);
    }

    const LONG cbAlign = NUMBYTES(DWORD);
    cbItem = ALIGN(cbItem, cbAlign);

    if (cGrowBy <= 0)
        cGrowBy = 4;
    if (cAllocate <= 0)
        cAllocate = 16;
    cAllocate = DivRU(cAllocate, cGrowBy) * cGrowBy;

    plst->cbItem     = cbItem;
	plst->cAllocated = cAllocate;
	plst->cGrowBy    = cGrowBy;

    LONG cb = cAllocate * cbItem;
    plst->pvList = (void**)malloc(cb);
    if (plst->pvList)
       ZeroMemory(plst->pvList, cb);

	if (fdwOptions & ARYLIST_OPT_F_EMBEDDED)
	{
		return plst->pvList ? S_OK : E_OUTOFMEMORY;
	}

    DASSERT(plst->pvList);
    if ( ! plst->pvList)
    {
        free(plst);
        return E_OUTOFMEMORY;
    }

    *phlst = (HARYLIST)plst;
    return S_OK;
}

HRESULT FNEXPORT HaryList_Destroy (HARYLIST hlst)
{
    PARYLIST plst = (PARYLIST)hlst;
    if ( ! plst)
        return S_OK;

    bool fSerialize = (plst->fdwOptions & ARYLIST_OPT_F_SERIALIZE) != 0;
    if (fSerialize)
        EnterCriticalSection(&plst->cs);

    if (plst->pvList)
    {
		if (plst->fdwOptions & ARYLIST_OPT_F_GPTRS)
		{
			LPVOID * ppv = (LPVOID*)plst->pvList;
			while (plst->cItems > 0)
			{
				LPVOID pv = ppv[plst->cItems-1];
				ppv[plst->cItems-1] = NULL;
				if (pv)
					free(pv);
				--plst->cItems;
			}
		}

        free(plst->pvList);
        plst->pvList = NULL;
    }

    if (fSerialize)
    {
        LeaveCriticalSection(&plst->cs);
        DeleteCriticalSection(&plst->cs);
    }

	if ( ! (plst->fdwOptions & ARYLIST_OPT_F_EMBEDDED))
		free(plst);
    return S_OK;
}

HRESULT HaryList_GrowAllocated (
    HARYLIST hlst,
    LONG     cDelta)
{
    PARYLIST plst = (PARYLIST)hlst;
    HRESULT hr = S_OK;

    bool fSerialize = (plst->fdwOptions & ARYLIST_OPT_F_SERIALIZE) != 0;
    if (fSerialize)
        EnterCriticalSection(&plst->cs);

    //
    //  grow by block aligned chunks
    //
    if (cDelta <= 0)
        cDelta = plst->cGrowBy;
    cDelta = DivRU(cDelta, plst->cGrowBy) * plst->cGrowBy;

    LONG cbNewAlloc = (plst->cAllocated + cDelta) * plst->cbItem;

    void** pvNew = (void**)malloc(cbNewAlloc);
    if ( ! pvNew)
    {
        hr = E_OUTOFMEMORY;
        goto bail;
    }

    ZeroMemory((char *)pvNew + plst->cAllocated * plst->cbItem, cbNewAlloc - plst->cAllocated * plst->cbItem);
    RtlCopyMemory(pvNew, plst->pvList, plst->cAllocated * plst->cbItem);
    free(plst->pvList);

    plst->pvList = pvNew;
    plst->cAllocated += cDelta;

bail:
    if (fSerialize)
        LeaveCriticalSection(&plst->cs);

    return hr;
}

HRESULT FNEXPORT HaryList_InsertList (
    HARYLIST  hlst,
    LONG      ixInsert,
    LPCVOID   pvItems,
    LONG      cItemsToInsert,
	LONG *    pixLoc) // location that the insertion happened.
{
    PARYLIST  plst = (PARYLIST)hlst;
    HRESULT   hr = S_OK;

    bool fSerialize = (plst->fdwOptions & ARYLIST_OPT_F_SERIALIZE) != 0;
    if (fSerialize)
		EnterCriticalSection(&plst->cs);
    
    //  !!! this is an _internal_ override !!!
    if (cItemsToInsert < 0)
        cItemsToInsert = -cItemsToInsert;

    // as a special case, we treat negative insertion points
	// as meaning append (insert on the end)
    //
    LONG cItems = HaryList_GetCount(plst);
    if (ixInsert < 0)
        ixInsert = cItems;

    DASSERT(ixInsert <= cItems);
    ixInsert = min(ixInsert, cItems);

    // create space for the new item if needed.
    //
    if (plst->cAllocated < cItems + cItemsToInsert)
    {
        hr = HaryList_GrowAllocated(hlst, cItemsToInsert);
        if (FAILED(hr))
            goto bail;
    }

    // move all items after the insertion point down by the
    // to make room for the iterms we want to insert.
    //
    {
    LONG cItemsToMove = cItems - ixInsert;
    if (cItemsToMove > 0)
    {
        const void *pvSrc = HaryList_RawItemAddr(hlst, ixInsert);
        LPVOID      pvDst = HaryList_RawItemAddr(hlst, ixInsert + cItemsToInsert);
        RtlMoveMemory(pvDst, pvSrc, cItemsToMove * plst->cbItem);
    }

    // copy the items to insert at the insertion point.
    //
    LPVOID pvDst = HaryList_RawItemAddr(hlst, ixInsert);
    RtlCopyMemory(pvDst, pvItems, cItemsToInsert * plst->cbItem);
    }

    // grow the count by the number of items we inserted.
    //
    plst->cItems += cItemsToInsert;

    if (pixLoc)
       *pixLoc = ixInsert;

bail:
    if (fSerialize)
        LeaveCriticalSection(&plst->cs);

    return hr;
}


#endif //
