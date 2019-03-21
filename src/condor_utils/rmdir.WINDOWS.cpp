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
#include <shlwapi.h>  // for PathXXX functions

#define NUMELMS(aa)         (sizeof(aa)/sizeof((aa)[0]))
#define NUMBYTES(aa)        (sizeof(aa))
#define NUMCHARS            NUMELMS

static LPWSTR strdupW(LPCWSTR psz)
{
   if ( ! psz) return NULL;
   size_t     cb = sizeof(WCHAR) * (lstrlenW(psz)+1);
   LPWSTR pszNew =  (LPWSTR)malloc(cb);
   CopyMemory(pszNew, psz, cb);
   return pszNew;
}


// -------------------------------------------------- bprint stuff ---------
//
typedef struct _bprint_buffer {
   int    flags;
   UINT   CodePage; // code page (ansi/oem)
   UINT   cchMax;
   UINT   cch;
   LPTSTR psz;
   } BPRINT_BUFFER;

static void bprint_Initialize(BPRINT_BUFFER & bp);
static bool bprint_Write(BPRINT_BUFFER & bp, LPCTSTR psz);
static bool bprint_EndLine(BPRINT_BUFFER & bp);
static int __cdecl bprintf(BPRINT_BUFFER & bp, LPCTSTR pszFormat, ...);
static void __cdecl bprintl(BPRINT_BUFFER & bp, LPCTSTR psz);

__inline void bprint_InitializeIfNot(BPRINT_BUFFER & bp) { if ( ! bp.psz) bprint_Initialize(bp); }
__inline bool bprint_Flush(BPRINT_BUFFER & bp) { return bprint_EndLine(bp); }
__inline int  bprint_Count(BPRINT_BUFFER & bp) { return bp.cch; }
__inline bool bprint_IsEmpty(BPRINT_BUFFER & bp) { return (bp.cch == 0); }
__inline bool bprint_Clear(BPRINT_BUFFER & bp)
   {
   bool fEmpty = bprint_IsEmpty(bp);
   bp.cch = 0;
   return ! fEmpty;
   }


static bool bprint_EndLine(BPRINT_BUFFER & bp)
{
   bool fAdded = false;
   if (bp.cch > 0 && bp.psz[bp.cch-1] != '\n')
      {
      bp.psz[bp.cch++] = '\n';
      bp.psz[bp.cch] = 0;
      fAdded = true;
      }
   bprint_Write(bp, NULL);
   return fAdded;
}

static bool bprint_Sep(BPRINT_BUFFER & bp, char ch)
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

static int __cdecl bprint(BPRINT_BUFFER & bp, LPCTSTR pszNew)
{
   int cchNew = lstrlen(pszNew);

   LPTSTR psz = bp.psz + bp.cch;
   if (bp.cch + cchNew > bp.cchMax)
   {
      DebugBreak();
      cchNew = (bp.cchMax - bp.cch);
   }

   CopyMemory(psz, pszNew, cchNew * NUMBYTES(pszNew[0]));

   if (cchNew > 0)
      bp.cch += cchNew;

   return cchNew;
}

static int __cdecl bprint(BPRINT_BUFFER & bp, LPCWSTR pszNew)
{
   int cchNew = lstrlenW(pszNew);

   LPTSTR psz = bp.psz + bp.cch;
   if (bp.cch + cchNew > bp.cchMax)
   {
      DebugBreak();
      cchNew = (bp.cchMax - bp.cch);
   }

   UINT cch = WideCharToMultiByte(CP_ACP, 0, pszNew, cchNew, psz, bp.cchMax - bp.cch, 0, 0);
   if (cch > 0)
      bp.cch += cch;

   return cch;
}

static int __cdecl bvprintf(BPRINT_BUFFER & bp, LPCTSTR pszFormat, va_list va)
{
   LPTSTR psz = bp.psz + bp.cch;
   if (bp.cch + 256 > bp.cchMax)
   {
      DebugBreak();
      return -1;
   }

   int cch = _vsnprintf(psz, bp.cchMax - bp.cch, pszFormat, va);
   if (cch > 0)
      bp.cch += cch;

   return cch;
}

static int __cdecl bprintf(BPRINT_BUFFER & bp, LPCTSTR pszFormat, ...)
{
   va_list va;
   va_start(va, pszFormat);
   int cch = bvprintf(bp, pszFormat, va);
   va_end(va);
   return cch;
}

static void __cdecl bprintl(BPRINT_BUFFER & bp, LPCTSTR psz)
{
   bprint(bp, psz);
   bprint_EndLine(bp);
}

static void __cdecl bprintfl(BPRINT_BUFFER & bp, LPCTSTR pszFormat, ...)
{
   va_list va;
   va_start(va, pszFormat);
   int cch = bvprintf(bp, pszFormat, va);
   va_end(va);

   bprint_EndLine(bp);
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

     #if 1
      dprintf(bp.flags, "%s", bp.psz);
     #else
      if (bp.hOut)
         {
         DWORD cbWrote = 0;
         dprintf(bp.hOut, bp.psz, bp.cch, &cbWrote, NULL);
         }
     #endif
      bp.cch = 0;
      return true;
      }
   return false;
}

void bprint_Initialize(BPRINT_BUFFER & bp)
{
   if (bp.cchMax <= 0)
      bp.cchMax = 0x0FFFF;
   if ( ! bp.psz)
      bp.psz = (LPTSTR) malloc((bp.cchMax+1) * NUMBYTES(bp.psz[0]));
   bp.cch = 0;
   bp.CodePage = CP_ACP;
}

void bprint_Terminate(BPRINT_BUFFER & bp, bool fFlush)
{
   if (fFlush)
      bprint_EndLine(bp);

   if (bp.psz)
   {
      LocalFree(bp.psz);
      bp.psz = NULL;
   }
}

static BPRINT_BUFFER g_bpErr = {D_ALWAYS, CP_UTF8};
static BPRINT_BUFFER g_bpDiag = {D_FULLDEBUG, CP_UTF8};
static BPRINT_BUFFER * g_pbpDiag = &g_bpDiag;

static void ReportError(int err, LPCSTR szContext, LPCTSTR pszPath=NULL)
{
   BPRINT_BUFFER & bp = g_bpErr;

   bprint_EndLine(bp);
   bprint(bp, szContext);
   if (pszPath)
      {
      bprint_Sep(bp, ' ');
      bprint(bp, pszPath);
      }
   bprint_Sep(bp, ' ');
   bprint(bp, TEXT("Failed : "));
   bprint_AppendErrorText(bp, err);
   bprint_EndLine(bp);
}

static void ReportErrorW(int err, LPCSTR szContext, LPCWSTR pszPath)
{
   BPRINT_BUFFER & bp = g_bpErr;

   bprint_EndLine(bp);
   bprint(bp, szContext);
   if (pszPath)
      {
      bprint_Sep(bp, ' ');
      bprint(bp, pszPath);
      }
   bprint_Sep(bp, ' ');
   bprint(bp, TEXT("Failed : "));
   bprint_AppendErrorText(bp, err);
   bprint_EndLine(bp);
}
// This enum much match the entries in the g_aPrivs table.
enum {
   ESE_SECURITY=0,
   ESE_IMPERSONATE,
   ESE_TAKE_OWNERSHIP,
   ESE_ENABLE_DELEGATION,
   ESE_CREATE_TOKEN,
   ESE_ASSIGNPRIMARYTOKEN,
   ESE_INCREASE_QUOTA,
   ESE_INC_BASE_PRIORITY,
   ESE_CHANGE_NOTIFY,
   ESE_SHUTDOWN,
   ESE_DEBUG,
   #ifdef SE_CREATE_SYMBOLIC_LINK_NAME
   ESE_CREATE_SYMBOLIC_LINK,
   #endif
   };

static struct _privtable {
   LPCTSTR psz;
   LUID    luid;
   } g_aPrivs[] = {
   SE_SECURITY_NAME,            0,0,
   SE_IMPERSONATE_NAME,         0,0,
   SE_TAKE_OWNERSHIP_NAME,      0,0,
   SE_ENABLE_DELEGATION_NAME,   0,0,
   SE_CREATE_TOKEN_NAME,        0,0,
   SE_ASSIGNPRIMARYTOKEN_NAME,  0,0,
   SE_INCREASE_QUOTA_NAME,      0,0,
   SE_INC_BASE_PRIORITY_NAME,   0,0,
   SE_CHANGE_NOTIFY_NAME,       0,0,
   SE_SHUTDOWN_NAME,            0,0,
   SE_DEBUG_NAME,               0,0,
   #ifdef SE_CREATE_SYMBOLIC_LINK_NAME
   SE_CREATE_SYMBOLIC_LINK_NAME,0,0,
   #endif
   };

static struct _aceslocalstate {
   HANDLE              hToken; // will be non-null if we had to get the token and change 

   PSID                AliasAdminsSid;
   PSID                SeWorldSid;
   PSID                SeSYSTEMSid;
   PSID                AuthUsersSid;
   //PSID                UserSid;
   PSID                OwnerSid;

   DWORD               cOrig;
   LUID_AND_ATTRIBUTES aOrig[NUMELMS(g_aPrivs)];
   DWORD               cCurr;
   LUID_AND_ATTRIBUTES aCurr[NUMELMS(g_aPrivs)];
   } ls = {NULL};

static SID_IDENTIFIER_AUTHORITY    SepNtAuthority = SECURITY_NT_AUTHORITY;
static SID_IDENTIFIER_AUTHORITY    SepWorldSidAuthority   = SECURITY_WORLD_SID_AUTHORITY;

int InitStandardSids(BOOL fDiagnostic, BPRINT_BUFFER *pbp)
{
   int err = 0;
   if ( ! ls.AliasAdminsSid &&
        ! AllocateAndInitializeSid(
                 &SepNtAuthority,
                 2,
                 SECURITY_BUILTIN_DOMAIN_RID,
                 DOMAIN_ALIAS_RID_ADMINS,
                 0,
                 0,
                 0,
                 0,
                 0,
                 0,
                 &ls.AliasAdminsSid))
      {
      err = GetLastError();
      }

   if ( ! ls.SeWorldSid &&
        ! AllocateAndInitializeSid(
                 &SepWorldSidAuthority,
                 1,
                 SECURITY_WORLD_RID,
                 0,
                 0,
                 0,
                 0,
                 0,
                 0,
                 0,
                 &ls.SeWorldSid))
      {
      err = GetLastError();
      }

   if ( ! ls.SeSYSTEMSid &&
        ! AllocateAndInitializeSid(
                 &SepNtAuthority,
                 1,
                 SECURITY_LOCAL_SYSTEM_RID,
                 0,
                 0,
                 0,
                 0,
                 0,
                 0,
                 0,
                 &ls.SeSYSTEMSid))
      {
      err = GetLastError();
      }

   if ( ! ls.AuthUsersSid &&
        ! AllocateAndInitializeSid(
                 &SepNtAuthority,
                 1,
                 SECURITY_AUTHENTICATED_USER_RID,
                 0,
                 0,
                 0,
                 0,
                 0,
                 0,
                 0,
                 &ls.AuthUsersSid))
      {
      err = GetLastError();
      }


   return err;
}

bool AddLocalPrivilege(int ePriv)
{
   if (ePriv < 0 || ePriv >= NUMELMS(g_aPrivs))
      {
      ASSERT(!"unsupported priv requested");
      return false;
      }

   if ( ! ls.hToken)
      {
      if ( ! OpenThreadToken(GetCurrentThread(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, TRUE, &ls.hToken))
         {
         if (GetLastError() == ERROR_NO_TOKEN)
            {
            if ( ! OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &ls.hToken))
               return false;
            }
         else
            return false;
         }
      }

   // have we queried this already?
   if (ls.aCurr[ePriv].Luid.LowPart || ls.aCurr[ePriv].Luid.HighPart)
      {
      return (ls.aCurr[ePriv].Attributes & SE_PRIVILEGE_ENABLED) != 0;
      }

   PLUID pluid = &g_aPrivs[ePriv].luid;
   if ( ! pluid->LowPart && ! pluid->HighPart &&
        ! LookupPrivilegeValue(NULL, g_aPrivs[ePriv].psz, &g_aPrivs[ePriv].luid))
      {
      return false;
      }

   TOKEN_PRIVILEGES tp = {1};
   tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
   tp.Privileges[0].Luid       = *pluid;

   TOKEN_PRIVILEGES tpOrig = {1};
   tpOrig.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
   tpOrig.Privileges[0].Luid       = *pluid;

   //
   // enable or disable the privilege
   //
   DWORD cb = sizeof(tpOrig);
   if ( ! AdjustTokenPrivileges(ls.hToken, FALSE, &tp, cb, &tpOrig, &cb))
      {
      bprintfl(*g_pbpDiag, " %d passed, %d needed", sizeof(tpOrig), cb);
      return false;
      }


   ls.aOrig[ePriv].Luid       = tpOrig.Privileges[0].Luid;
   ls.aOrig[ePriv].Attributes = tpOrig.Privileges[0].Attributes;
   ls.aCurr[ePriv].Luid       = tp.Privileges[0].Luid;
   ls.aCurr[ePriv].Attributes = tp.Privileges[0].Attributes;


   return true;
}

static void PrintSidName(PSID psid, BPRINT_BUFFER & bp)
{
   TCHAR szDomain[32]; // max domain name is actually 15
   DWORD cchDomain = NUMCHARS(szDomain);
   TCHAR szName[MAX_PATH];
   DWORD cchName = NUMCHARS(szName);
   SID_NAME_USE snu;

   szDomain[0] = szName[0] = 0;
   if ( ! LookupAccountSid(NULL, psid, szName, &cchName, szDomain, &cchDomain, &snu))
      {
      if (GetLastError() != ERROR_NONE_MAPPED)
         ReportError(GetLastError(), "LookupAccountSid");
      }
   else
      {
      if (szDomain[0])
         {
         bprint(bp, szDomain);
         bprint(bp, "\\");
         }
      bprint(bp, szName);
      }
}

static void PrintSidText(PSID psid, BPRINT_BUFFER & bp)
{
   //
   // test if parameters passed in are valid, IsValidSid can not take
   // a NULL parameter
   //
   if ( ! psid)
      return;

   // obtain SidIdentifierAuthority
   // obtain sidsubauthority count
   PSID_IDENTIFIER_AUTHORITY psia = GetSidIdentifierAuthority(psid);
   DWORD cSubAs = *GetSidSubAuthorityCount(psid);

   //
   // S-SID_REVISION-
   //
   bprintf(bp, TEXT("S-%lu-"), SID_REVISION);

   //
   // append SidIdentifierAuthority
   //
   if (psia->Value[0] || psia->Value[1]) 
      {
      bprintf(bp, TEXT("0x%02hx%02hx%02hx%02hx%02hx%02hx"),
             (USHORT)psia->Value[0],
             (USHORT)psia->Value[1],
             (USHORT)psia->Value[2],
             (USHORT)psia->Value[3],
             (USHORT)psia->Value[4],
             (USHORT)psia->Value[5]);
      }
   else
      {
      bprintf(bp, TEXT("%lu"),
             (ULONG)(psia->Value[5]      ) +
             (ULONG)(psia->Value[4] <<  8) +
             (ULONG)(psia->Value[3] << 16) +
             (ULONG)(psia->Value[2] << 24));
      }

   //
   // append SidSubAuthorities
   //
   for (UINT ix = 0 ; ix < cSubAs; ++ix) 
      {
      bprintf(bp, TEXT("-%lu"), *GetSidSubAuthority(psid, ix));
      }
}

int InitOwnerSid(BOOL fDiagnostic, BPRINT_BUFFER *pbp)
{
   int err = 0;
   if ( ! ls.OwnerSid)
      {
      LPVOID pvInfo = NULL;
      DWORD cbSize = 0;
      if ( ! GetTokenInformation(ls.hToken, TokenOwner,  NULL, 0, &cbSize))
         {
         err = GetLastError();
         if (ERROR_INSUFFICIENT_BUFFER == err)
            {
            err = 0;
            pvInfo = LocalAlloc(LPTR, cbSize);
            if ( ! pvInfo)
               {
               err = ERROR_OUTOFMEMORY;
               }
            else if ( ! GetTokenInformation(ls.hToken, TokenOwner,  pvInfo, cbSize, &cbSize))
               {
               err = GetLastError();
               ReportError(err, "GetTokenInformation(TokenOwner)");
               }
            else
               {
               err = 0;
               const TOKEN_OWNER * pOwner = (const TOKEN_OWNER *)pvInfo;
               ls.OwnerSid = pOwner->Owner;
               if (fDiagnostic)
                  {
                  bprintf(*pbp, TEXT("Got Owner Sid "));
                  PrintSidName(ls.OwnerSid, *pbp);
                  bprint_Sep(*pbp, ' ');
                  PrintSidText(ls.OwnerSid, *pbp);
                  bprint_EndLine(*pbp);
                  }
               }
            }
         }
      }

   return err;
}

int RemoveFileDACLs(LPCWSTR pszPath, BOOL fDiagnostic, BPRINT_BUFFER *pbp)
{
   //
   // make sure that we have ESE_SECURITY priviledge
   //
   int err = 0;
   if ( ! AddLocalPrivilege(ESE_SECURITY))
      {
      err = GetLastError();
      if (fDiagnostic)
         ReportError(err, "AdjustTokenPrivileges");
      }

   // setup globals, this do nothing if this is not the first time
   // this function was called. 
   //
   InitStandardSids(fDiagnostic, pbp);
   //InitUserSid(fDiagnostic, pbp);
   InitOwnerSid(fDiagnostic, pbp);

   //
   // Attempt to put a NULL Dacl on the file/directory
   //
   SECURITY_DESCRIPTOR si;
   ZeroMemory(&si, sizeof(si));
   InitializeSecurityDescriptor(&si, SECURITY_DESCRIPTOR_REVISION);
   MSC_SUPPRESS_WARNING(6248) // warning: setting the DACL to null will result in unprotected object...
   SetSecurityDescriptorDacl (&si,  TRUE, NULL, FALSE);

   if ( ! SetFileSecurityW(pszPath, DACL_SECURITY_INFORMATION, &si))
      {
      err = GetLastError();
      if (fDiagnostic)
         ReportErrorW (err, "SetFileSecurity(DACL)[1] ", pszPath);
      }
   else
      {
      if (fDiagnostic)
         {
         if (bprint_IsEmpty(*pbp))
            bprintfl(*pbp, "DACLs removed from %s", pszPath);
         else
            bprint(*pbp, " DACLs removed");
         }
      return 0;
      }

   //
   // Attempt to make take ownership of the file.
   //

   SetSecurityDescriptorOwner (&si, ls.OwnerSid, FALSE);

   if (SetFileSecurityW(pszPath, OWNER_SECURITY_INFORMATION, &si))
      err = 0;
   else
      {
      err = GetLastError();
      if (fDiagnostic)
         ReportErrorW (err, "SetFileSecurity(Owner)[1] ", pszPath);

      //ZeroMemory(&si,sizeof(si));
      //InitializeSecurityDescriptor(&si, SECURITY_DESCRIPTOR_REVISION);
      //SetSecurityDescriptorOwner (&si, ls.OwnerSid, FALSE);

      if ( ! AddLocalPrivilege(ESE_TAKE_OWNERSHIP))
         {
         static bool fReportedSeTakeOwner = false;
         if (fDiagnostic && ! fReportedSeTakeOwner)
            {
            ReportErrorW(GetLastError(), "SeTakeOwnership ", pszPath);
            fReportedSeTakeOwner = true;
            }
         }
      else if ( ! SetFileSecurityW(pszPath, OWNER_SECURITY_INFORMATION, &si))
         {
         err = GetLastError();
         if (fDiagnostic)
            ReportErrorW (err, "SetFileSecurity(Owner)[2] ", pszPath);
         }
      else
         {
         err = 0;
         }
      }

   // if we successfully took ownership, try again to set a NULL DACL 
   //
   if ( ! err)
      {
      if (fDiagnostic)
         {
         if (bprint_IsEmpty(*pbp))
            bprintfl(*pbp, "Ownership taken of %s", pszPath);
         else
            bprint(*pbp, " Ownership taken");
         }

      if ( ! SetFileSecurityW(pszPath, DACL_SECURITY_INFORMATION, &si))
         {
         err = GetLastError();
         if (fDiagnostic)
            ReportErrorW (err, "SetFileSecurity(DACL)[2] ", pszPath);
         }
      else
         {
         if (fDiagnostic)
            {
            if (bprint_IsEmpty(*pbp))
               bprintfl(*pbp, "DACLs removed from %s", pszPath);
            else
               bprintfl(*pbp, " DACLs removed", pszPath);
            }
         return 0;
         }
      }

   return err; 
}

static bool IsDotOrDotDot(LPCWSTR psz)
{
   if (psz[0] != '.') return false;
   if (psz[1] == 0) return true;
   return (psz[1] == '.' && psz[2] == 0);
}

typedef struct _linked_find_data {
   struct _linked_find_data * next;
   WIN32_FIND_DATAW           wfd;
}  LinkedFindData;

template <class T>
T * ReverseLinkedList(T * first) 
{
   T * new_first = NULL;
   while (first) 
      {
      T * next = first->next;
      first->next = new_first;
      new_first = first;
      first = next;
      }
   return new_first;
}

typedef bool (WINAPI *PFNTraverseDirectoryTreeCallbackW)(
   VOID *  pvUser, 
   LPCWSTR pszPath,    // path and filename, may be absolute or relative.
   int     ochName,    // offset from start of pszPath to first char of the file/dir name.
   DWORD   fdwFlags,
   int     nCurrentDepth,              
   int     ixCurrentItem,
   const WIN32_FIND_DATAW & wfd);

// Flags values for TraverseDirectoryTree
//
#define TDT_SUBDIRS    0x0001
#define TDT_NODIRS     0x0002
#define TDT_NOFILES    0x0004
#define TDT_CONTINUE   0x0008
#define TDT_DIAGNOSTIC 0x0010
#define TDT_DIRFIRST   0x0020
#define TDT_DIRLAST    0x0040  // directory callback is after callbacks of all it's children.

// These flags are set and cleared by the directory traversal engine to indicate things
// about the flow of the traversal, not it is not a bug that TDT_DIRLAST == TDT_LAST
//
#define TDT_LAST       0x0040  // this is the directory callback AFTER all child callbacks.
#define TDT_FIRST      0x0080  // this is the first callback in a given directory

#define TDT_INPUTMASK  0xFFFF001F // These flags can be passed in to TraverseDirectoryTree
#define TDT_USERMASK   0xFFFF0000 // These flags are passed on to the callback unmodified. 

int TraverseDirectoryTreeW (
   LPCWSTR pszPath, 
   LPCWSTR pszPattern, 
   DWORD   fdwFlags, 
   PFNTraverseDirectoryTreeCallbackW pfnCallback, 
   LPVOID  pvUser,
   int     nCurrentDepth)
{
   bool fSubdirs    = (fdwFlags & TDT_SUBDIRS) != 0;
   bool fDiagnostic = (fdwFlags & TDT_DIAGNOSTIC) != 0;
   //bool fDirFirst   = (fdwFlags & TDT_DIRFIRST) != 0;
   //bool fDirLast    = (fdwFlags & TDT_DIRLAST) != 0;

   if (fDiagnostic)
      {
      if (pszPattern)
         bprintfl(*g_pbpDiag, "TDT[%d]: %04x path = '%s' pattern = '%s' subdirs = %d", nCurrentDepth, fdwFlags, pszPath, pszPattern, fSubdirs);
      else
         bprintfl(*g_pbpDiag, "TDT[%d]: %04x path = '%s' pattern = NULL subdirs = %d", nCurrentDepth, fdwFlags, pszPath, fSubdirs);
      }

   // build a string containing path\pattern, we will pass this
   // into FindFirstFile. there are some special cases.
   // we treat paths that end in \ as meaning path\*.
   // we replace *.* with * since it means the same thing and
   // is easier to parse. (this is a special case go back to DOS). 
   //
   int cchPath = lstrlenW(pszPath);
   int cchMax = cchPath + MAX_PATH + 2;
   LPWSTR psz = (LPWSTR)malloc(cchMax * sizeof(WCHAR));
   lstrcpyW(psz, pszPath);
   LPWSTR pszNext = psz + lstrlenW(psz);

   bool fMatchAll = false;
   if (pszPattern && pszPattern[0])
      {
      if (0 == lstrcmpW(pszPattern, L"*.*") || 0 == lstrcmpW(pszPattern, L"*"))
         {
         fMatchAll = true;
         }
      pszNext = PathAddBackslashW(psz);
      lstrcpyW(pszNext, pszPattern);
      }
   else if (pszNext > psz && (pszNext[-1] == '\\' || pszNext[-1] == '/'))
      {
      fMatchAll = true;
      pszNext[0] = '*';
      pszNext[1] = 0;
      }

   HRESULT hr = S_OK;
   int err = ERROR_SUCCESS;
   int ixCurrentItem = 0;
   DWORD dwFirst = TDT_FIRST;

   LinkedFindData * pdirs = NULL;

   WIN32_FIND_DATAW wfd;
   HANDLE hFind = FindFirstFileW(psz, &wfd);
   if (INVALID_HANDLE_VALUE == hFind) 
      {
      err = GetLastError();
      // if we can't open the directory because of an access denied error
      // it might be the DACLs that are causing the problem. If so, then
      // just remove the dacls. we are going to be deleting the directory
      // anway...
      //
      if (ERROR_ACCESS_DENIED == err)
         {
         int errT = RemoveFileDACLs(pszPath, fdwFlags & TDT_DIAGNOSTIC, g_pbpDiag);
         if (errT)
            err = errT;
         else
            {
            hFind = FindFirstFileW(psz, &wfd);
            if (INVALID_HANDLE_VALUE == hFind) 
               {
               errT = GetLastError();
               if (errT != err)
                  ReportErrorW(errT, "FindFirstFile ", psz);
               }
            else
               {
               err = 0;
               }
            }
         }
      if (err)
         ReportErrorW(err, "FindFirstFile ", psz);
      }

   if (hFind && INVALID_HANDLE_VALUE != hFind)
      {
      do {
         // ignore . and ..
         if (IsDotOrDotDot(wfd.cFileName))
            continue;

         ++ixCurrentItem;

         bool fSkip = false;
         if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
            // if we are recursing, and we happen to be matching all, then remember this
            // directory for later (so we don't need to enumerate again for dirs).
            // otherwise we will have to re-enumerate in this directory to get subdirs.
            if (fSubdirs && fMatchAll) 
               {
               LinkedFindData * pdir = (LinkedFindData*)malloc(sizeof(LinkedFindData));
               pdir->wfd = wfd;
               pdir->next = pdirs;
               pdirs = pdir;
               fSkip = true;  // we will do the callback for this directory later, if at all.
               }
            else if (fdwFlags & TDT_NODIRS)
               fSkip = true;
            }
         else
            {
            if (fdwFlags & TDT_NOFILES)
               fSkip = true;
            }

         if (fDiagnostic)
            bprintfl (*g_pbpDiag, "TDT[%d]: 0x%08x %s %s", nCurrentDepth, wfd.dwFileAttributes, wfd.cFileName, fSkip ? "<skip>" : "");

         if ( ! fSkip)
            {
            lstrcpyW(pszNext, wfd.cFileName); 
            if ( ! pfnCallback(pvUser, psz, pszNext - psz, (fdwFlags & ~(TDT_DIRLAST | TDT_DIRFIRST)) | dwFirst, nCurrentDepth, ixCurrentItem, wfd))
               break;
            dwFirst = 0;
            }

         } while (FindNextFileW(hFind, &wfd));

         if (fDiagnostic)
            bprintfl (*g_pbpDiag, "TDT[%d]: Done with %s %s", nCurrentDepth, pszPath, pszPattern);
      }

   // we want to traverse subdirs, but we were unable to build a list of dirs when we 
   // enumerated the files, so re-enumerate with * to get the directories.
   if (fSubdirs && ! fMatchAll)
      {
      if (hFind && INVALID_HANDLE_VALUE != hFind)
         FindClose(hFind);

      lstrcpyW(pszNext, L"*");
      hFind = FindFirstFileW(psz, &wfd);
      if (INVALID_HANDLE_VALUE != hFind) 
         {
         do {
            if ((wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && ! IsDotOrDotDot(wfd.cFileName))
               {
               LinkedFindData * pdir = (LinkedFindData*)malloc(sizeof(LinkedFindData));
               pdir->wfd = wfd;
               pdir->next = pdirs;
               pdirs = pdir;
               }

            } while (FindNextFileW(hFind, &wfd));
         }
      }

   err = GetLastError();
   if (ERROR_NO_MORE_FILES == err)
      err = ERROR_SUCCESS;

   // now traverse and callback subdirectories.
   //
   if (fSubdirs && pdirs)
      {
      pdirs = ReverseLinkedList(pdirs);
      while (pdirs)
         {
         LinkedFindData * pdir = pdirs;
         pdirs = pdirs->next;

         lstrcpyW(pszNext, pdir->wfd.cFileName); 
         if ((fdwFlags & TDT_DIRFIRST) && ! (fdwFlags & TDT_NODIRS))
            {
            if (fDiagnostic)
               bprintfl (*g_pbpDiag, "TDT[%d]: DIRFIRST 0x%08x %s %s", nCurrentDepth, pszPath, pszPattern);

            if ( ! pfnCallback(pvUser, psz, pszNext - psz, TDT_DIRFIRST | (fdwFlags & ~TDT_DIRLAST) | dwFirst, nCurrentDepth, ixCurrentItem, pdir->wfd))
               break;
            dwFirst = 0;
            }

         err = TraverseDirectoryTreeW(psz, pszPattern, fdwFlags, pfnCallback, pvUser, nCurrentDepth+1);

         if ((fdwFlags & TDT_DIRLAST) && ! (fdwFlags & TDT_NODIRS))
            {
            if (fDiagnostic)
               bprintfl (*g_pbpDiag, "TDT[%d]: DIRLAST 0x%08x %s %s", nCurrentDepth, pszPath, pszPattern);

            if ( ! pfnCallback(pvUser, psz, pszNext - psz, TDT_DIRLAST | (fdwFlags & ~TDT_DIRFIRST) | dwFirst, nCurrentDepth, ixCurrentItem, pdir->wfd))
               break;
            dwFirst = 0;
            }
         }
      }


   if (hFind && INVALID_HANDLE_VALUE != hFind)
      FindClose(hFind);

   return err;
}

typedef struct _deletepathdata {

   int   cMaxDepth;
   int   cLastDepth;
   int   cTotalVisited;
   int   cDirsVisited;
   int   cFilesVisited;

   int   cFilesDeleted;
   int   cDirsRemoved;
   int   cDeleteFailures;
   int   cRemoveFailures;

   BPRINT_BUFFER * pbp;

   bool fDiagnostic;
   bool fNoDelete;
   bool fAborting;
   bool fDirAlreadyTriedDACLRemove;

} DeletePathData;
#define DPD_F_REMOVEDACLS       0x0001

static int WINAPI DpdDeleteFile (
   LPCWSTR          pszPath,
   int              ochName,
   DeletePathData & dpd,
   DWORD            fdwVerbose,
   DWORD            fdwFlags)
{
   int err = 0;
   if (dpd.fNoDelete)
      {
      if (fdwVerbose)
         bprintf(*dpd.pbp, TEXT("NoDelete %s "), pszPath);
      return 0;
      }

   if (fdwVerbose)
      bprintf(*dpd.pbp, TEXT("Deleting %s "), pszPath);

   if (DeleteFileW(pszPath))
      {
      dpd.cFilesDeleted += 1;
      }
   else
      {
      err = GetLastError();
      if ((ERROR_ACCESS_DENIED == err) && (fdwFlags & DPD_F_REMOVEDACLS))
         {
         if ( ! dpd.fDirAlreadyTriedDACLRemove && (ochName > 1))
            {
            LPWSTR pszParent = strdupW(pszPath);
            pszParent[ochName-1] = 0;
            //StrCopyN(szParent, NUMCHARS(szParent), pszPath, ochName);
            int errT = RemoveFileDACLs(pszParent, dpd.fDiagnostic, dpd.pbp);
            if (errT)
               err = errT;
            else if (DeleteFileW(pszPath))
               {
               err = 0;
               dpd.cFilesDeleted += 1;
               }
            else
               {
               err = GetLastError();
               }
            // dont try to remove dacls again for this directory.
            dpd.fDirAlreadyTriedDACLRemove = true;
            free(pszParent);
            }

         // the error _may_ have been cleared by the time we get here, if it hasn't
         // then try and remove dacls from the file, then try again to delete the file.
         //
         if (ERROR_ACCESS_DENIED == err)
            {
            int errT = RemoveFileDACLs(pszPath, dpd.fDiagnostic, dpd.pbp);
            if (errT)
               err = errT;
            else if (DeleteFileW(pszPath))
               {
               err = 0;
               dpd.cFilesDeleted += 1;
               }
            else
               {
               err = GetLastError();
               }
            }
         }

      if (err)
         {
         dpd.cDeleteFailures += 1;
         if (fdwVerbose)
            {
            bprint_Sep(*dpd.pbp, ' ');
            bprint_AppendErrorText(*dpd.pbp, err);
            }
         }
      }

   return err;
}

static int WINAPI DpdRemoveDirectory (
   LPCWSTR          pszPath,
   int              ochName,
   DeletePathData & dpd,
   DWORD            fdwVerbose,
   DWORD            fdwFlags)
{
   int err = 0;
   if (dpd.fNoDelete)
      {
      if (fdwVerbose)
         bprintf(*dpd.pbp, TEXT("NoRemove %s "), pszPath);
      return 0;
      }

   if (fdwVerbose)
      bprintf(*dpd.pbp, TEXT("Removing %s "), pszPath);

   if (RemoveDirectoryW(pszPath))
      {
      dpd.cDirsRemoved += 1;
      }
   else
      {
      err = GetLastError();
      if ((ERROR_ACCESS_DENIED == err) && (fdwFlags & DPD_F_REMOVEDACLS))
         {
         if ( ! dpd.fDirAlreadyTriedDACLRemove && (ochName > 1))
            {
            LPWSTR pszParent = strdupW(pszPath);
            pszParent[ochName-1] = 0;
            //StrCopyN(szParent, NUMCHARS(szParent), pszPath, ochName);
            int errT = RemoveFileDACLs(pszParent, dpd.fDiagnostic, dpd.pbp);
            if (errT)
               err = errT;
            else if (RemoveDirectoryW(pszPath))
               {
               err = 0;
               dpd.cDirsRemoved += 1;
               }
            else
               {
               err = GetLastError();
               }
            // dont try to remove dacls again for this directory.
            dpd.fDirAlreadyTriedDACLRemove = true;
            free(pszParent);
            }

         // the error _may_ have been cleared by the time we get here, if it hasn't
         // then try and remove dacls from the file, then try again to delete the file.
         //
         if (ERROR_ACCESS_DENIED == err)
            {
            int errT = RemoveFileDACLs(pszPath, dpd.fDiagnostic, dpd.pbp);
            if (errT)
               err = errT;
            else if (RemoveDirectoryW(pszPath))
               {
               err = 0;
               dpd.cDirsRemoved += 1;
               }
            else
               {
               err = GetLastError();
               }
            }
         }

      if (err)
         {
         dpd.cRemoveFailures += 1;
         if (fdwVerbose)
            {
            bprint_Sep(*dpd.pbp, ' ');
            bprint_AppendErrorText(*dpd.pbp, err);
            }
         }
      }
   return err;
}

static bool WINAPI DeletePathCallback (
   VOID *  pvUser, 
   LPCWSTR pszPath,   // path and filename, may be absolute or relative.
   int     ochName,   // offset from start of pszPath to first char of the file/dir name.
   DWORD   fdwFlags,
   int     cDepth,
   int     ixItem,
   const WIN32_FIND_DATAW & wfd)
{
   DeletePathData & dpd = *(DeletePathData*)pvUser;
   if (dpd.fAborting)
      return false;

   BPRINT_BUFFER & bp = *dpd.pbp;
   const bool fDirectory = (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
   const bool fProtected = (wfd.dwFileAttributes & (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_SYSTEM)) != 0;

   dpd.cTotalVisited += 1;
   if (fDirectory)
      dpd.cDirsVisited += 1;
   else
      dpd.cFilesVisited += 1;

   dpd.cLastDepth = cDepth;
   dpd.cMaxDepth = MAX(cDepth, dpd.cMaxDepth);

   if (fProtected)
      {
      // build attributes flags that don't contain readonly or system (or hidden)
      DWORD fdwAttr = wfd.dwFileAttributes & (FILE_ATTRIBUTE_OFFLINE | FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_NOT_CONTENT_INDEXED | FILE_ATTRIBUTE_TEMPORARY);
      if ((fdwFlags & TDT_DIAGNOSTIC) || dpd.fDiagnostic)
         bprintfl(*dpd.pbp, TEXT("Clearing%s%s attrib(s) from %s"), 
                 (wfd.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) ? TEXT(" SYS") : TEXT(""),
                 (wfd.dwFileAttributes & FILE_ATTRIBUTE_READONLY) ? TEXT(" RO") : TEXT(""),
                 pszPath);

      if ( ! SetFileAttributesW(pszPath,  fdwAttr))
         {
         int err = GetLastError();
         if (ERROR_ACCESS_DENIED == err)
            {
            int errT = RemoveFileDACLs(pszPath, dpd.fDiagnostic, dpd.pbp);
            if (errT)
               err = errT;
            else if (SetFileAttributesW(pszPath, fdwAttr))
               err = 0;
            else
               err = GetLastError();
            }
         }
      }

   if (fdwFlags & TDT_FIRST)
      dpd.fDirAlreadyTriedDACLRemove = false;

   bprint_EndLine(bp);
   int err = ERROR_SUCCESS;
   if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
      {
      // When we get the DIRLAST callback, it should be after we have deleted all of the children.
      // NOW we can try and remove the directory.
      // 
      if (fdwFlags & TDT_DIRLAST)
         {
         err = DpdRemoveDirectory(pszPath, ochName, dpd, dpd.fDiagnostic, DPD_F_REMOVEDACLS);
         }
      }
   else
      {
      err = DpdDeleteFile(pszPath, ochName, dpd, dpd.fDiagnostic, DPD_F_REMOVEDACLS);
      }

   if (err && ! (fdwFlags & TDT_CONTINUE))
      {
      bprint_EndLine(bp);
      if (dpd.fDiagnostic)
         bprintl(bp, "Aborting.");
      dpd.fAborting = true;
      return false;
      }

   bprint_EndLine(bp);
   return true;
}

static int RecursiveDeleteDirectory(LPCTSTR pszPathIn, DWORD fdwFlags)
{
   BPRINT_BUFFER & bp = *g_pbpDiag;
   DeletePathData dpd;
   ZeroMemory(&dpd, sizeof(dpd));
   dpd.pbp = &g_bpErr;

   //this is for traversal testing...
   //dpd.fNoDelete = true;

   fdwFlags |= TDT_DIRLAST; // So we get directory callbacks after children have been deleted.

   // if the path isn't too long, convert to canonical form.  for long paths
   // they has better provide canonical form to begin with.
   //
   TCHAR szCanonPath[MAX_PATH+1];
   LPCTSTR pszPath = pszPathIn;
   if (lstrlen(pszPathIn) < MAX_PATH)
      {
      if (PathCanonicalize(szCanonPath, pszPathIn))
         pszPath = szCanonPath;      
      if (PathIsRelative(pszPath))
         return ERROR_INVALID_PARAMETER;
      }

   // the unicode versions of the windows file api's can handle very long
   // path names if they are canonical and if they are preceeded by \\?\.
   // if we are build unicode, take advantage of that capability.
   static const WCHAR szPre[]= L"\\\\?\\";
   WCHAR szFullPath[MAX_PATH + NUMCHARS(szPre)];
   WCHAR * pszFullPath = szFullPath;
   WCHAR * pszFilePart = NULL;

   lstrcpyW(szFullPath, szPre);
   MultiByteToWideChar(CP_ACP, 0, pszPath, -1, szFullPath + NUMCHARS(szPre)-1, MAX_PATH);
   /*
   UINT cchFullPath = GetFullPathNameW(pszPath, MAX_PATH, szFullPath + NUMCHARS(szPre)-1, &pszFilePart);
   if (cchFullPath >= NUMCHARS(szFullPath)-1)
      {
      pszFullPath = (TCHAR *)LocalAlloc(LPTR, (cchFullPath + NUMCHARS(szPre) + 2) * sizeof(WCHAR));
      if (pszFullPath)
         {
         lstrcpyW(pszFullPath, szPre);
         if (GetFullPathNameW(pszPath, cchFullPath+2, pszFullPath + NUMCHARS(szPre)-1, &pszFilePart) <= cchFullPath+2)
            pszPath = pszFullPath;
         }
      }
   else
      pszPath = szFullPath;
   */

   int err = 0;
   DWORD fdwFileAttributes = GetFileAttributesW(szFullPath);
   if (INVALID_FILE_ATTRIBUTES == fdwFileAttributes)
      {
      fdwFileAttributes = 0;
      err = GetLastError();
      ReportError(err, "Could not get attributes for ", pszPath);
      return err;
      }
   const bool fIsDir = (fdwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

   if (fdwFlags & TDT_DIAGNOSTIC)
      bprintfl(bp, TEXT("Delete %s 0x%04X %s\\*"), fIsDir ? TEXT("Directory") : TEXT("File"), fdwFlags, pszPath);

   if (fIsDir)
      {
      err = TraverseDirectoryTreeW(szFullPath, L"*", fdwFlags, DeletePathCallback, &dpd, 0);
      if ( ! err || (fdwFlags & TDT_CONTINUE))
         {
         bprint_EndLine(bp);
         if ( ! dpd.fNoDelete && ! RemoveDirectoryW(szFullPath))
            {
            bprint_EndLine(bp);
            err = GetLastError();
            ReportError(err, "RemoveDirectory", pszPath);
            }
         }
      }
   else // initial path is a file, not a directory. 
      {
      if ( ! dpd.fNoDelete &&  ! DeleteFileW(szFullPath))
         {
         bprint_EndLine(bp);
         err = GetLastError();
         ReportError(err, "DeleteFile", pszPath);
         }
      }

   bprint_EndLine(bp);
   return err;
}

int rmdir_with_acls_win32(const char * path)
{
   bprint_InitializeIfNot(g_bpErr);
   bprint_InitializeIfNot(g_bpDiag);
   int err = RecursiveDeleteDirectory(path, TDT_SUBDIRS | TDT_CONTINUE);   
   bprint_Flush(g_bpDiag);
   bprint_Flush(g_bpErr);
   return err;
}

#ifdef RMDIR_OBJ_DEBUG

extern void set_debug_flags( const char *strflags, int flags );
extern "C" FILE	*DebugFP;

// Main method for testing the Perm functions
int 
main(int argc, char* argv[]) {
	
	char *filename, *domain, *username;
	perm* foo = new perm();

	DebugFP = stdout;
	set_debug_flags( "D_ALL", 0 );
	//char p_ntdomain[80];
	//char buf[100];
	
	if (argc < 4) { 
		cout << "Usage:\nperm filename domain username" << endl;
		return (1);
	}
	filename = argv[1];
	domain = argv[2];
	username = argv[3];
	
	foo->init(username, domain);
	
	cout << "Checking write access for " << filename << endl;
	
	int result = foo->write_access(filename);
	
	if ( result == 1) {
		cout << "You can write to " << filename << endl;
	}
	else if (result == 0) {
		cout << "You are not allowed to write to " << filename << endl;
	}
	else if (result == -1) {
		cout << "An error has occured\n";
	}
	else {
		cout << "Ok you're screwed" << endl;
	}
	delete foo;
	return(0);
}

#endif
