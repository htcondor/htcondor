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

// -------------------------------------------------- bprint stuff ---------
//
typedef struct _bprint_buffer {
   UINT   cch;
   UINT   cchMax;
   BOOL   fUnicode;
   UINT   CodePage; // code page (ansi/oem)
   HANDLE hOut;    // stdout or output file.
   LPTSTR psz;
   LPSTR  pszAscii;
   } BPRINT_BUFFER;

#define printf freddy
int __cdecl freddy(int ii, ...);

void bprint_Terminate(BPRINT_BUFFER & bp, bool fFlush);
void bprint_Initialize(BPRINT_BUFFER & bp);
void bprint_Initialize();
int __cdecl bprint(BPRINT_BUFFER & bp, LPCTSTR pszNew);
int __cdecl bprint_cch(BPRINT_BUFFER & bp, LPCTSTR pszNew, int cchNew);
int __cdecl bprint_PadRight(BPRINT_BUFFER & bp, LPCTSTR pszNew, int cch, TCHAR ch);
int __cdecl bvprintf(BPRINT_BUFFER & bp, LPCTSTR pszFormat, va_list va);
int __cdecl bprintf(BPRINT_BUFFER & bp, LPCTSTR pszFormat, ...);
bool bprint_Write(BPRINT_BUFFER & bp, LPCTSTR psz);
bool bprint_EndLine(BPRINT_BUFFER & bp);
bool bprint_Sep(BPRINT_BUFFER & bp, LPCTSTR psz);
bool bprint_Sep(BPRINT_BUFFER & bp, char ch);
void __cdecl bprintfl(BPRINT_BUFFER & bp, LPCTSTR pszFormat, ...);
void __cdecl bprintl(BPRINT_BUFFER & bp, LPCTSTR psz);
int  bprint_Move(BPRINT_BUFFER & bpTo, BPRINT_BUFFER & bpFrom);
int bprint_AppendErrorText(BPRINT_BUFFER & bp, int err);

#ifndef NO_GLOBAL_BPRINT
int __cdecl bprintf(LPCTSTR pszFormat, ...);
void __cdecl bprintfl(LPCTSTR pszFormat, ...);
void __cdecl bprintl(LPCTSTR psz);
#endif


#ifdef UNICODE
int __cdecl bprint(BPRINT_BUFFER & bp, LPCSTR pszNew);
int __cdecl bprintf(LPCSTR pszFormat, ...);
int __cdecl bprint(LPCSTR psz);
void __cdecl bprintfl(BPRINT_BUFFER & bp, LPCSTR pszFormat, ...);
void __cdecl bprintfl(LPCSTR pszFormat, ...);
void __cdecl bprintl(BPRINT_BUFFER & bp, LPCSTR psz);
void __cdecl bprintl(LPCSTR psz);
#endif //UNICODE

__inline int  bprint_Count(BPRINT_BUFFER & bp) { return bp.cch; }
__inline bool bprint_IsEmpty(BPRINT_BUFFER & bp) { return (bp.cch == 0); }
__inline bool bprint_Clear(BPRINT_BUFFER & bp)
   {
   bool fEmpty = bprint_IsEmpty(bp);
   bp.cch = 0;
   return ! fEmpty;
   }

//extern BPRINT_BUFFER g_bprint;

#ifdef BPRINT_INCLUDE_CODE

#if defined BPRINT_USE_STRSAFE
 #include <strsafe.h> // for StrCchVPrintF
#elif defined BPRINT_USE_CRT
 #include <stdio.h>   // for _vsnprintf
#else
 #include <Shlwapi.h> // for wvnsprintf
 #pragma comment(lib,"shlwapi.lib")
#endif


BPRINT_BUFFER g_bprint = {0, 0x10000};

int __cdecl bprint(BPRINT_BUFFER & bp, LPCTSTR pszNew)
{
   int cchNew = lstrlen(pszNew);

   LPTSTR psz = bp.psz + bp.cch;
   if (bp.cch + cchNew > bp.cchMax)
   {
      DebugBreak();
      cchNew = (bp.cchMax - bp.cch);
   }

   RtlCopyMemory(psz, pszNew, cchNew * NUMBYTES(pszNew[0]));

   if (cchNew > 0)
      bp.cch += cchNew;

   return cchNew;
}

int __cdecl bprint_cch(BPRINT_BUFFER & bp, LPCTSTR pszNew, int cchNew)
{
   LPTSTR psz = bp.psz + bp.cch;
   if (bp.cch + cchNew > bp.cchMax)
   {
      DebugBreak();
      cchNew = (bp.cchMax - bp.cch);
   }

   RtlCopyMemory(psz, pszNew, cchNew * NUMBYTES(pszNew[0]));

   if (cchNew > 0)
      bp.cch += cchNew;

   return cchNew;
}

int __cdecl bprint_PadRight(BPRINT_BUFFER & bp, LPCTSTR pszNew, int cch, TCHAR ch)
{
   int cchNew = lstrlen(pszNew);
   int cchTot = max(cchNew, cch);

   LPTSTR psz = bp.psz + bp.cch;
   if (bp.cch + cchTot > bp.cchMax)
   {
      DebugBreak();
      cchTot = (bp.cchMax - bp.cch);
      if (cchNew > cchTot)
         cchNew = cchTot;
   }

   RtlCopyMemory(psz, pszNew, cchNew * NUMBYTES(pszNew[0]));

   while (cchNew < cchTot)
       psz[cchNew++] = ch;

   if (cchNew > 0)
      bp.cch += cchNew;

   return cchNew;
}

#ifdef UNICODE
int __cdecl bprint(BPRINT_BUFFER & bp, LPCSTR pszNew)
{
   int cchNew = lstrlenA(pszNew);

   LPTSTR psz = bp.psz + bp.cch;
   if (bp.cch + cchNew > bp.cchMax)
   {
      DebugBreak();
      cchNew = (bp.cchMax - bp.cch);
   }

   UINT cch = MultiByteToWideChar(CP_ACP, 0, pszNew, cchNew, psz, bp.cchMax - bp.cch);
   if (cch > 0)
      bp.cch += cch;

   return cch;
}
#endif

int __cdecl bvprintf(BPRINT_BUFFER & bp, LPCTSTR pszFormat, va_list va)
{
   LPTSTR psz = bp.psz + bp.cch;
   if (bp.cch + 256 > bp.cchMax)
   {
      DebugBreak();
      return -1;
   }

  #if !defined BPRINT_USE_STRSAFE && !defined BPRINT_USE_CRT
   int cch = wvnsprintf(psz, bp.cchMax - bp.cch, pszFormat, va);
  #elif defined BPRINT_USE_STRSAFE
    int cch = StringCchVPrintF(psz, bp.cchMax - bp.cch, pszFormat, va);
  #elif defined BPRINT_USE_CRT
   #ifdef UNICODE
    int cch = _vsnwprintf(psz, bp.cchMax - bp.cch, pszFormat, va);
   #else
    int cch = _vsnprintf(psz, bp.cchMax - bp.cch, pszFormat, va);
   #endif
  #else
   #pragma error "no vprintf method defined. bvprintf will not work!
  #endif
   if (cch > 0)
      bp.cch += cch;

   return cch;
}

int __cdecl bprintf(BPRINT_BUFFER & bp, LPCTSTR pszFormat, ...)
{
   va_list va;
   va_start(va, pszFormat);
   int cch = bvprintf(bp, pszFormat, va);
   va_end(va);

   return cch;
}
int __cdecl bprintf(LPCTSTR pszFormat, ...)
{
   va_list va;
   va_start(va, pszFormat);
   int cch = bvprintf(g_bprint, pszFormat, va);
   va_end(va);

   return cch;
}

#ifdef UNICODE

// can't use this with libctiny because of the 8kb buffer on the stack.
// which generates a reference to __chkstk in the c-runtime...
int __cdecl bprintf(LPCSTR pszFormat, ...)
{
   int cch = -1;
   va_list va;
   va_start(va, pszFormat);

   // stack useage > 4k generate a reference to __chkstk which libctiny doesn't have.
  #ifdef USE_LIBCTINY
   TCHAR wszFormat[2000];
  #else
   TCHAR wszFormat[8192];
  #endif
   MultiByteToWideChar(CP_ACP, 0, pszFormat, -1, wszFormat, NUMCHARS(wszFormat));
   cch = bvprintf(g_bprint, wszFormat, va);

   va_end(va);

   return cch;
}

int __cdecl bprint(LPCSTR psz)
{
   return bprint(g_bprint, psz);
}
#endif

void bprint_Initialize(BPRINT_BUFFER & bp)
{
   if (bp.cchMax <= 0)
      bp.cchMax = 0x0FFFF;
   if ( ! bp.psz)
      bp.psz = (LPTSTR) malloc((bp.cchMax+1) * NUMBYTES(bp.psz[0]));
   if (bp.psz)
      bp.psz[0] = 0;
  #ifdef UNICODE
   if ( ! bp.pszAscii)
      bp.pszAscii = (LPSTR) malloc((bp.cchMax+1) * 2);
   if (bp.pszAscii)
      bp.pszAscii[0] = 0;
  #endif
   bp.cch = 0;
   bp.CodePage = CP_ACP;
}

void bprint_Initialize()
{
   bprint_Initialize(g_bprint); 
   g_bprint.hOut = GetStdHandle(STD_OUTPUT_HANDLE);
   g_bprint.CodePage = CP_OEMCP;
}

bool bprint_Write(BPRINT_BUFFER & bp, LPCTSTR psz)
{
   if (psz)
      bprint(bp, psz);

   if (bp.cch > 0)
      {
      if (bp.cch > bp.cchMax)
         {
         DebugBreak();
         bp.cch = bp.cchMax;
         }

      bp.psz[bp.cch] = 0;

      if (bp.hOut)
         {
         DWORD cbWrote = 0;
        #ifdef UNICODE
         if (bp.fUnicode)
            WriteFile(bp.hOut, bp.psz, bp.cch * NUMBYTES(bp.psz[0]), &cbWrote, NULL);
         else
            {
            WideCharToMultiByte(bp.CodePage, 0, bp.psz, bp.cch, bp.pszAscii, (bp.cchMax+1)*2, NULL, NULL);
            WriteFile(bp.hOut, bp.pszAscii, bp.cch * NUMBYTES(bp.pszAscii[0]), &cbWrote, NULL);
            }
        #else
         WriteFile(bp.hOut, bp.psz, bp.cch, &cbWrote, NULL);
        #endif
         }
      bp.cch = 0;
      return true;
      }
   return false;
}

bool bprint_EndLine(BPRINT_BUFFER & bp)
{
   bool fAdded = false;
   if (bp.cch > 0 && bp.psz[bp.cch-1] != _UT('\n'))
      {
      bp.psz[bp.cch++] = _UT('\n');
      bp.psz[bp.cch] = 0;
      fAdded = true;
      }
   bprint_Write(bp, NULL);
   return fAdded;
}

// add a separator if there is already data and if
// the separator is not already there.
//
bool bprint_Sep(BPRINT_BUFFER & bp, LPCTSTR psz)
{
   bool fAdded = false;
   int cch = lstrlen(psz);
   if (cch <= 0)
      return false;
   if ((int)bp.cch >= cch && CompareMemory((const char*)psz, (const char*)&bp.psz[bp.cch-cch], cch*sizeof(TCHAR)))
      {
      bprint(bp, psz);
      fAdded = true;
      }
   return fAdded;
}

bool bprint_Sep(BPRINT_BUFFER & bp, char ch)
{
   bool fAdded = false;
   if ((int)bp.cch >= 1 && bp.psz[bp.cch-1] != ch)
      {
      bp.psz[bp.cch++] = ch;
      bp.psz[bp.cch] = 0;
      fAdded = true;
      }
   return fAdded;
}

int bprint_Move(BPRINT_BUFFER & bpTo, BPRINT_BUFFER & bpFrom)
{
   int cch = 0;
   if ( ! bprint_IsEmpty(bpFrom))
      {
      cch = bprint_cch(bpTo, bpFrom.psz, bpFrom.cch);
      bprint_Clear(bpFrom);
      }
   return cch;
}

int bprint_AppendErrorText(BPRINT_BUFFER & bp, int err)
{
   int cch = FormatMessage(FORMAT_MESSAGE_IGNORE_INSERTS
                           | FORMAT_MESSAGE_FROM_SYSTEM,
                           0, err, 0, 
                           &bp.psz[bp.cch], bp.cchMax - bp.cch,
                           NULL);
   if (cch > 0)
      {
      bp.cch += cch;
      bp.psz[bp.cch] = 0;
      return bp.cch;
      }

   return bprintf(bp, TEXT("Error %d"), err);
}


void __cdecl bprintfl(BPRINT_BUFFER & bp, LPCTSTR pszFormat, ...)
{
   va_list va;
   va_start(va, pszFormat);
   int cch = bvprintf(bp, pszFormat, va);
   va_end(va);

   bprint_EndLine(bp);
}
void __cdecl bprintfl(LPCTSTR pszFormat, ...)
{
   va_list va;
   va_start(va, pszFormat);
   int cch = bvprintf(g_bprint, pszFormat, va);
   va_end(va);

   bprint_EndLine(g_bprint);
}

void __cdecl bprintl(BPRINT_BUFFER & bp, LPCTSTR psz)
{
   bprint(bp, psz);
   bprint_EndLine(bp);
}

void __cdecl bprintl(LPCTSTR psz)
{
   bprint(g_bprint, psz);
   bprint_EndLine(g_bprint);
}

#ifdef UNICODE
void __cdecl bprintfl(BPRINT_BUFFER & bp, LPCSTR pszFormat, ...)
{
   int cch = -1;
   va_list va;
   va_start(va, pszFormat);

   // stack useage > 4k generate a reference to __chkstk which libctiny doesn't have.
  #ifdef USE_LIBCTINY
   TCHAR wszFormat[2000];
  #else
   TCHAR wszFormat[8192];
  #endif
   MultiByteToWideChar(CP_ACP, 0, pszFormat, -1, wszFormat, NUMCHARS(wszFormat));
   cch = bvprintf(bp, wszFormat, va);

   va_end(va);

   bprint_EndLine(bp);
}

void __cdecl bprintfl(LPCSTR pszFormat, ...)
{
   int cch = -1;
   va_list va;
   va_start(va, pszFormat);

   // stack useage > 4k generate a reference to __chkstk which libctiny doesn't have.
  #ifdef USE_LIBCTINY
   TCHAR wszFormat[2000];
  #else
   TCHAR wszFormat[8192];
  #endif
   MultiByteToWideChar(CP_ACP, 0, pszFormat, -1, wszFormat, NUMCHARS(wszFormat));
   cch = bvprintf(g_bprint, wszFormat, va);

   va_end(va);

   bprint_EndLine(g_bprint);
}

void __cdecl bprintl(BPRINT_BUFFER & bp, LPCSTR psz)
{
   bprint(bp, psz);
   bprint_EndLine(bp);
}

void __cdecl bprintl(LPCSTR psz)
{
   bprint(g_bprint, psz);
   bprint_EndLine(g_bprint);
}
#endif // UNICODE

void bprint_Terminate(BPRINT_BUFFER & bp, bool fFlush)
{
   if (fFlush)
      bprint_EndLine(bp);
   if (bp.psz)
      free(bp.psz);
   if (bp.pszAscii)
      free(bp.pszAscii);
   if (bp.hOut && (bp.hOut != GetStdHandle(STD_OUTPUT_HANDLE)))
      CloseHandle(bp.hOut);
   ZeroStruct(&bp);
}
#endif // BPRINT_INCLUDE_CODE

#if defined BPRINT_INCLUDE_CODE || !defined NO_GLOBAL_BPRINT
__inline void bprint_Terminate(bool fFlush) { bprint_Terminate(g_bprint, fFlush); }
__inline int bprint_Count()   { return bprint_Count(g_bprint); }
__inline int bprint_IsEmpty() { return bprint_IsEmpty(g_bprint); }
__inline bool bprint_Clear()  { return bprint_Clear(g_bprint); }
__inline bool bprint_Write(BPRINT_BUFFER & bp) { return bprint_Write(bp, NULL); }
__inline bool bprint_Write() { return bprint_Write(g_bprint, NULL); }
__inline bool bprint_EndLineG() { return bprint_EndLine(g_bprint); }
#endif
