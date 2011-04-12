/***************************************************************
 *
 * Copyright (C) 2011, Condor Team, Computer Sciences Department,
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

#pragma once
#define _COMMON_H_

//#define USE_LIBCTINY 1
//#undef MoveMemory
//#undef RtlMoveMemory
//extern "C" __declspec(dllimport) void* __stdcall RtlMoveMemory(void *, const void *, int);
//#pragma comment(linker,"/EXPORT:RtlMoveMemory=Kernel32.RtlMoveMemory")

#ifndef NUMELMS
#define NUMELMS(aa)         (sizeof(aa)/sizeof((aa)[0]))
#endif
#define NUMCHARS(x)         NUMELMS(x)
#define NUMBYTES(x)         sizeof(x)

#define VERIFY(a) ((void)(a))
#define DASSERT(a) ((void)0)
#define INLINE __inline
#define FNEXPORT _declspec(dllexport) CALLBACK

#ifdef _X86_
 #define INLINE_BREAK _asm {int 3}
#else
 #define INLINE_BREAK DebugBreak()
#endif

#ifndef REMIND
 #define QUOTE(x)   #x
 #define QQUOTE(y)  QUOTE(y)
 #define REMIND(str) message(__FILE__ "(" QQUOTE(__LINE__) ") : " str)
#endif

#ifndef NUMELMS
#define NUMELMS(aa)         (sizeof(aa)/sizeof((aa)[0]))
#endif
#define NUMCHARS(x)         NUMELMS(x)
#define NUMBYTES(x)         sizeof(x)
#define NUMWIDES(x)         (sizeof(x) / sizeof(WCHAR))

#define ZeroStruct(ps)        RtlZeroMemory(ps, sizeof(*(ps)))

#define BOUND(x, l, h)          (((x) < (l)) ? (l) : ((x) > (h) ? (h) : (x)))

#ifndef IS_WITHIN
//
// IS_WITHIN and IS_OUTSIDE treat the end (second parameter) as
// inclusive. while IS_WITHIN_RGN and IS_OUTSIDE_RGN treat it as
// exclusive.
// For example IS_WITHIN(2,0,2) is true while IS_WITHIN_RGN(2,0,2) is false
//
#define IS_WITHIN(x, start, end)    (((x) >= (start)) && ((x) <= (end)))
#define IS_WITHIN_RGN(x,start,max)  (((x) >= (start)) && ((x) < (max)))
#define IS_WITHIN_LEN(x,start,len)  (IS_WITHIN_RGN(x,start,(start)+(len)))
#define IS_OUTSIDE(x, start, end)   (((x) < (start)) || ((x) > (end)))
#define IS_OUTSIDE_RGN(x,start,max) (((x) < (start)) || ((x) >= (max)))
#define IS_OUTSIDE_LEN(x,start,len) (IS_OUTSIDE_RGN(x,start,(start)+(len)))
//#define IS_FLAG_SET(flags, mask)    (0 != ((mask) & (flags)))
//#define IS_FLAG_CLEAR(flags, mask)  (0 == ((mask) & (flags)))
//#define IS_MASK_SET(flags, mask)    ((mask) == ((mask) & (flags)))
//#define IS_MASK_CLEAR(flags, mask)  (0 == ((mask) & (flags)))
//#define IS_BIT_SET(flags, bit)      (0 != ((1L << (bit)) & (flags)))
//#define IS_BIT_CLEAR(flags, bit)    (0 == ((1L << (bit)) & (flags)))
#endif

#define IsCurrentThread(tid) (GetCurrentThreadId() == (tid))

int CompareMemory(const char * pb1, const char * pb2, int cb);

// shlwapi and strsafe both define this. I want to use it
#undef StrCopy

#define E_CANCEL               MAKE_SCODE(SEVERITY_ERROR, FACILITY_ITF, 0xD000 + 1)
#define E_FILEOPEN             MAKE_SCODE(SEVERITY_ERROR, FACILITY_ITF, 0xD000 + 10)
#define E_FILE_UNSUPPORTED     MAKE_SCODE(SEVERITY_ERROR, FACILITY_ITF, 0xD000 + 11)
#define E_INSUFFICIENT_BUFFER  MAKE_SCODE(SEVERITY_ERROR, FACILITY_ITF, 0xD000 + 12)

#define STR_MAX_POSSIBLE_CCH   2147483647  // max buffer size, in characters, that we support (same as INT_MAX)
#define STR_MAX_LENGTH         (STR_MAX_POSSIBLE_CCH - 1)   // max buffer length, in characters, that we support

template <class T> 
HRESULT StrCopyWorker(
    T*   pszDest,
    int  cchDest,
    int* pcchNewDestLength,
    const T* pszSrc,
    int  cchToCopy)
{
    HRESULT hr = S_OK;
    int cchNewDestLength = 0;

    while (cchDest && cchToCopy && *pszSrc) {
        *pszDest++ = *pszSrc++;
        cchDest--;
        cchToCopy--;
        cchNewDestLength++;
    }

    // we havn't copied the terminating null yet, so if
    // cchDest is 0, we are going to truncate
    if (0 == cchDest) {
        pszDest--;
        cchNewDestLength--;

        hr = E_INSUFFICIENT_BUFFER;
    }

    *pszDest = 0;
    if (pcchNewDestLength)
        *pcchNewDestLength = cchNewDestLength;

    return hr;
}


#define _UT(sz) TEXT(sz)
#define StrLen(psz) lstrlen(psz)
#define StrIsEqualI(psz1,psz2) (0 == lstrcmpi(psz1, psz2))
template <class T> T StrCharNext(T psz) { if ( ! *psz) return psz; return CharNext(psz); }
template <class T> INLINE bool CharIsSpace(T ch) { return (' ' == ch) || (ch >= 0x09 && ch <= 0x0D); }
template <class T> UINT StrAllocBytes(T psz) { return (UINT)(psz ? lstrlen(psz)+1 : 0) * sizeof(psz[0]); }
template <class T> T StrSkipWhiteSpace(T psz) { while(CharIsSpace(psz[0])) { psz = StrCharNext(psz); } return psz; }

template <class T> void StrCopy(T * psz1, const T * psz2, int cch1Max) {
   StrCopyWorker(psz1, cch1Max, NULL, psz2, STR_MAX_POSSIBLE_CCH);
   }
template <class T> void StrCopyN(T * psz1, int cch1Max, const T * psz2, int cchToCopy) {
   StrCopyWorker(psz1, cch1Max, NULL, psz2, cchToCopy); 
   }

#ifndef FIELDOFF
  #define FIELDOFF(st,fld) FIELD_OFFSET(st, fld)
  #define FIELDSIZ(st,fld) ((UINT)(sizeof(((st *)0)->fld)))
#endif
