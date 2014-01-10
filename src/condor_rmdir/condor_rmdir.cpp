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

#define UNICODE
#include <windows.h>
#include <shlwapi.h>
#include "common.h"

#define NO_GLOBAL_BPRINT
#include "bprint.h"
#include "condor_rmdir.h"

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
   PSID                UserSid;
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
      DASSERT(!"unsupported priv requested");
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

int InitUserSid(BOOL fDiagnostic, BPRINT_BUFFER *pbp)
{
   int err = 0;
   if ( ! ls.UserSid)
      {
      LPVOID pvInfo = NULL;
      DWORD cbSize = 0;
      if ( ! GetTokenInformation(ls.hToken, TokenUser,  NULL, 0, &cbSize))
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
            else if ( ! GetTokenInformation(ls.hToken, TokenUser,  pvInfo, cbSize, &cbSize))
               {
               err = GetLastError();
               ReportError(err, "GetTokenInformation(TokenUser)");
               }
            else
               {
               err = 0;
               const TOKEN_USER * pUser = (const TOKEN_USER *)pvInfo;
               ls.UserSid = pUser->User.Sid;
               if (fDiagnostic)
                  {
                  bprintf(*pbp, TEXT("Got User Sid "));
                  PrintSidName(ls.UserSid, *pbp);
                  bprint_Sep(*pbp, ' ');
                  PrintSidText(ls.UserSid, *pbp);
                  bprint_EndLine(*pbp);
                  }
               }
            }
         }
      }

   return err;
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

int RemoveFileDACLs(LPCTSTR pszPath, BOOL fDiagnostic, BPRINT_BUFFER *pbp)
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
   InitUserSid(fDiagnostic, pbp);
   InitOwnerSid(fDiagnostic, pbp);

   //
   // Attempt to put a NULL Dacl on the file/directory
   //
   SECURITY_DESCRIPTOR si;
   ZeroStruct(&si);
   InitializeSecurityDescriptor(&si, SECURITY_DESCRIPTOR_REVISION);
   #pragma warning(suppress: 6248) //  Setting a SECURITY_DESCRIPTOR's DACL to NULL will result in an unprotected object
   SetSecurityDescriptorDacl (&si,  TRUE, NULL, FALSE);

   if ( ! SetFileSecurity(pszPath, DACL_SECURITY_INFORMATION, &si))
      {
      err = GetLastError();
      if (fDiagnostic)
         ReportError (err, "SetFileSecurity(DACL)[1] ", pszPath);
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

   if (SetFileSecurity(pszPath, OWNER_SECURITY_INFORMATION, &si))
      err = 0;
   else
      {
      err = GetLastError();
      if (fDiagnostic)
         ReportError (err, "SetFileSecurity(Owner)[1] ", pszPath);

      //ZeroStruct(&si);
      //InitializeSecurityDescriptor(&si, SECURITY_DESCRIPTOR_REVISION);
      //SetSecurityDescriptorOwner (&si, ls.OwnerSid, FALSE);

      if ( ! AddLocalPrivilege(ESE_TAKE_OWNERSHIP))
         {
         static bool fReportedSeTakeOwner = false;
         if (fDiagnostic && ! fReportedSeTakeOwner)
            {
            ReportError(GetLastError(), "SeTakeOwnership ", pszPath);
            fReportedSeTakeOwner = true;
            }
         }
      else if ( ! SetFileSecurity(pszPath, OWNER_SECURITY_INFORMATION, &si))
         {
         err = GetLastError();
         if (fDiagnostic)
            ReportError (err, "SetFileSecurity(Owner)[2] ", pszPath);
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

      if ( ! SetFileSecurity(pszPath, DACL_SECURITY_INFORMATION, &si))
         {
         err = GetLastError();
         if (fDiagnostic)
            ReportError (err, "SetFileSecurity(DACL)[2] ", pszPath);
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

static bool IsDotOrDotDot(LPCTSTR psz)
{
   if (psz[0] != '.') return false;
   if (psz[1] == 0) return true;
   return (psz[1] == '.' && psz[2] == 0);
}

typedef struct _linked_find_data {
   struct _linked_find_data * next;
   WIN32_FIND_DATA            wfd;
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


extern BPRINT_BUFFER * g_pbpDiag;

int TraverseDirectoryTree (
   LPCTSTR pszPath, 
   LPCTSTR pszPattern, 
   DWORD   fdwFlags, 
   PFNTraverseDirectoryTreeCallback pfnCallback, 
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
   int cchPath = lstrlen(pszPath);
   int cchMax = cchPath + MAX_PATH + 2;
   LPTSTR psz = (LPTSTR)malloc(cchMax * sizeof(TCHAR));
   lstrcpy(psz, pszPath);
   LPTSTR pszNext = psz + lstrlen(psz);

   bool fMatchAll = false;
   if (pszPattern && pszPattern[0])
      {
      if (0 == lstrcmp(pszPattern, TEXT("*.*")) || 0 == lstrcmp(pszPattern, TEXT("*")))
         {
         fMatchAll = true;
         }
      pszNext = PathAddBackslash(psz);
      lstrcpy(pszNext, pszPattern);
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

   WIN32_FIND_DATA wfd;
   HANDLE hFind = FindFirstFile(psz, &wfd);
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
            hFind = FindFirstFile(psz, &wfd);
            if (INVALID_HANDLE_VALUE == hFind) 
               {
               errT = GetLastError();
               if (errT != err)
                  ReportError(errT, "FindFirstFile ", psz);
               }
            else
               {
               err = 0;
               }
            }
         }
      if (err)
         ReportError(err, "FindFirstFile ", psz);
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
            lstrcpy(pszNext, wfd.cFileName); 
            if ( ! pfnCallback(pvUser, psz, pszNext - psz, (fdwFlags & ~(TDT_DIRLAST | TDT_DIRFIRST)) | dwFirst, nCurrentDepth, ixCurrentItem, wfd))
               break;
            dwFirst = 0;
            }

         } while (FindNextFile(hFind, &wfd));

         if (fDiagnostic)
            bprintfl (*g_pbpDiag, "TDT[%d]: Done with %s %s", nCurrentDepth, pszPath, pszPattern);
      }

   // we want to traverse subdirs, but we were unable to build a list of dirs when we 
   // enumerated the files, so re-enumerate with * to get the directories.
   if (fSubdirs && ! fMatchAll)
      {
      if (hFind && INVALID_HANDLE_VALUE != hFind)
         FindClose(hFind);

      lstrcpy(pszNext, TEXT("*"));
      hFind = FindFirstFile(psz, &wfd);
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

            } while (FindNextFile(hFind, &wfd));
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

         lstrcpy(pszNext, pdir->wfd.cFileName); 
         if ((fdwFlags & TDT_DIRFIRST) && ! (fdwFlags & TDT_NODIRS))
            {
            if (fDiagnostic)
               bprintfl (*g_pbpDiag, "TDT[%d]: DIRFIRST 0x%08x %s %s", nCurrentDepth, pszPath, pszPattern);

            if ( ! pfnCallback(pvUser, psz, pszNext - psz, TDT_DIRFIRST | (fdwFlags & ~TDT_DIRLAST) | dwFirst, nCurrentDepth, ixCurrentItem, pdir->wfd))
               break;
            dwFirst = 0;
            }

         err = TraverseDirectoryTree(psz, pszPattern, fdwFlags, pfnCallback, pvUser, nCurrentDepth+1);

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

   bool fNoDelete;
   bool fAborting;
   bool fDirAlreadyTriedDACLRemove;

} DeletePathData;

#define TDT_USER_F_VERBOSE      0x00010000
#define DPD_F_REMOVEDACLS       0x0001

static int WINAPI DpdDeleteFile (
   LPCTSTR          pszPath,
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

   if (DeleteFile(pszPath))
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
            TCHAR szParent[MAX_PATH];
            StrCopyN(szParent, NUMCHARS(szParent), pszPath, ochName);
            int errT = RemoveFileDACLs(szParent, fdwVerbose & (TDT_DIAGNOSTIC | TDT_USER_F_VERBOSE), dpd.pbp);
            if (errT)
               err = errT;
            else if (DeleteFile(pszPath))
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
            }

         // the error _may_ have been cleared by the time we get here, if it hasn't
         // then try and remove dacls from the file, then try again to delete the file.
         //
         if (ERROR_ACCESS_DENIED == err)
            {
            int errT = RemoveFileDACLs(pszPath, fdwVerbose & (TDT_DIAGNOSTIC | TDT_USER_F_VERBOSE), dpd.pbp);
            if (errT)
               err = errT;
            else if (DeleteFile(pszPath))
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
   LPCTSTR          pszPath,
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

   if (RemoveDirectory(pszPath))
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
            TCHAR szParent[MAX_PATH];
            StrCopyN(szParent, NUMCHARS(szParent), pszPath, ochName);
            int errT = RemoveFileDACLs(szParent, fdwVerbose & (TDT_DIAGNOSTIC | TDT_USER_F_VERBOSE), dpd.pbp);
            if (errT)
               err = errT;
            else if (RemoveDirectory(pszPath))
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
            }

         // the error _may_ have been cleared by the time we get here, if it hasn't
         // then try and remove dacls from the file, then try again to delete the file.
         //
         if (ERROR_ACCESS_DENIED == err)
            {
            int errT = RemoveFileDACLs(pszPath, fdwVerbose & (TDT_DIAGNOSTIC | TDT_USER_F_VERBOSE), dpd.pbp);
            if (errT)
               err = errT;
            else if (RemoveDirectory(pszPath))
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
   LPCTSTR pszPath,   // path and filename, may be absolute or relative.
   int     ochName,   // offset from start of pszPath to first char of the file/dir name.
   DWORD   fdwFlags,
   int     cDepth,
   int     ixItem,
   const WIN32_FIND_DATA & wfd)
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
   dpd.cMaxDepth = max(cDepth, dpd.cMaxDepth);

   if (fProtected)
      {
      // build attributes flags that don't contain readonly or system (or hidden)
      DWORD fdwAttr = wfd.dwFileAttributes & (FILE_ATTRIBUTE_OFFLINE | FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_NOT_CONTENT_INDEXED | FILE_ATTRIBUTE_TEMPORARY);
      if (fdwFlags & (TDT_USER_F_VERBOSE | TDT_DIAGNOSTIC))
         bprintfl(*dpd.pbp, TEXT("Clearing%s%s attrib(s) from %s"), 
                 (wfd.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) ? TEXT(" SYS") : TEXT(""),
                 (wfd.dwFileAttributes & FILE_ATTRIBUTE_READONLY) ? TEXT(" RO") : TEXT(""),
                 pszPath);

      if ( ! SetFileAttributes(pszPath,  fdwAttr))
         {
         int err = GetLastError();
         if (ERROR_ACCESS_DENIED == err)
            {
            int errT = RemoveFileDACLs(pszPath, fdwFlags & (TDT_DIAGNOSTIC | TDT_USER_F_VERBOSE), dpd.pbp);
            if (errT)
               err = errT;
            else if (SetFileAttributes(pszPath, fdwAttr))
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
         err = DpdRemoveDirectory(pszPath, ochName, dpd, fdwFlags & (TDT_USER_F_VERBOSE | TDT_DIAGNOSTIC), DPD_F_REMOVEDACLS);
         }
      }
   else
      {
      err = DpdDeleteFile(pszPath, ochName, dpd, fdwFlags & (TDT_USER_F_VERBOSE | TDT_DIAGNOSTIC), DPD_F_REMOVEDACLS);
      }

   if (err && ! (fdwFlags & TDT_CONTINUE))
      {
      bprint_EndLine(bp);
      if (fdwFlags & (TDT_USER_F_VERBOSE | TDT_DIAGNOSTIC))
         bprintl(bp, "Aborting.");
      dpd.fAborting = true;
      return false;
      }

   bprint_EndLine(bp);
   return true;
}

int DeletePath(LPCTSTR pszPathIn, 
               LPCTSTR pszPattern, 
               DWORD   fdwTraverse, 
               DWORD   fdwSecurity, 
               DWORD   fdwVerbose, 
               BPRINT_BUFFER & bp)
{
   DWORD fdwFlags = fdwTraverse & (TDT_INPUTMASK & ~TDT_USERMASK);
   if (fdwVerbose & DP_V_VERBOSE)
      fdwFlags |= TDT_USER_F_VERBOSE;
   if (fdwVerbose & DP_V_DIAGNOSTIC)
      fdwFlags |= TDT_DIAGNOSTIC;

   bool fQuiet = (fdwVerbose & DP_V_QUIET) != 0;

   DeletePathData lad;
   ZeroStruct(&lad);
   lad.pbp = &bp;

   if (fdwSecurity & DP_S_NODELETE)
      lad.fNoDelete = true;

   fdwFlags |= TDT_DIRLAST; // So we get directory callbacks after children have been deleted.

   // if the path isn't too long, convert to canonical form.  for long paths
   // they has better provide canonical form to begin with.
   //
   TCHAR szCanonPath[MAX_PATH+1];
   LPCTSTR pszPath = pszPathIn;
   if (lstrlen(pszPathIn) < MAX_PATH &&
       PathCanonicalize(szCanonPath, pszPathIn))
      {
      pszPath = szCanonPath;      
      }

   // the unicode versions of the windows file api's can handle very long
   // path names if they are canonical and if they are preceeded by \\?\.
   // if we are build unicode, take advantage of that capability.
  #ifdef UNICODE
   static const TCHAR szPre[]= TEXT("\\\\?\\");
   TCHAR szFullPath[MAX_PATH + NUMCHARS(szPre)];
   TCHAR * pszFullPath = szFullPath;
   TCHAR * pszFilePart = NULL;

   lstrcpy(szFullPath, szPre);
   UINT cchFullPath = GetFullPathName(pszPath, MAX_PATH, szFullPath + NUMCHARS(szPre)-1, &pszFilePart);
   if (cchFullPath >= NUMCHARS(szFullPath)-1)
      {
      pszFullPath = (TCHAR *)LocalAlloc(LPTR, (cchFullPath + NUMCHARS(szPre) + 2) * sizeof(TCHAR));
      if (pszFullPath)
         {
         lstrcpy(pszFullPath, szPre);
         if (GetFullPathName(pszPath, cchFullPath+2, pszFullPath + NUMCHARS(szPre)-1, &pszFilePart) <= cchFullPath+2)
            pszPath = pszFullPath;
         }
      }
   else
      pszPath = szFullPath;
  #endif

   int err = 0;
   DWORD fdwFileAttributes = GetFileAttributes(pszPath);
   if (INVALID_FILE_ATTRIBUTES == fdwFileAttributes)
      {
      fdwFileAttributes = 0;
      err = GetLastError();
      ReportError(err, "Could not get attributes for ", pszPath);
      return err;
      }
   const bool fIsDir = (fdwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

   if (fdwFlags & TDT_DIAGNOSTIC)
      bprintfl(bp, TEXT("Delete %s 0x%04X %s\\%s"), fIsDir ? TEXT("Directory") : TEXT("File"), fdwFlags, pszPath, pszPattern);

   if (fIsDir)
      {
      if (fdwTraverse & TDT_SUBDIRS)
         {
         if ( ! pszPattern || ! pszPattern[0])
            err = TraverseDirectoryTree(pszPath, TEXT("*"), fdwFlags, DeletePathCallback, &lad, 0);
         else
            err = TraverseDirectoryTree(pszPath, pszPattern, fdwFlags, DeletePathCallback, &lad, 0);
         }

      if ( ! err || (fdwFlags & TDT_CONTINUE))
         {
         bprint_EndLine(bp);
         if ( ! (fdwTraverse & TDT_NODIRS) && (!pszPattern || !pszPattern[0]))
            {
            if ( ! fQuiet)
               {
               bprint(bp, lad.fNoDelete ? TEXT("NoRemove ") : TEXT("Removing "));
               bprint(bp, pszPath);
               }
            if ( ! lad.fNoDelete && ! RemoveDirectory(pszPath))
               {
               bprint_EndLine(bp);
               err = GetLastError();
               ReportError(err, "RemoveDirectory", pszPath);
               if ( ! (fdwFlags & TDT_CONTINUE))
                  if ( ! fQuiet) bprintl(bp, "Aborting.");
               }
            }
         }
      }
   else // initial path is a file, not a directory. 
      {
      if (fdwTraverse & TDT_SUBDIRS)
         {
         if ( ! pszPattern || ! pszPattern[0])
            {
            err = TraverseDirectoryTree(pszPath, TEXT("*"), fdwFlags, DeletePathCallback, &lad, 0);
            }
         else
            {
            #pragma REMIND("TJ: add code to split the file from the path and recurse on the filename.")
            }
         }
      if (fdwTraverse & TDT_NOFILES)
         {
         // Nothing to do.
         }
      else if ( ! pszPattern || ! pszPattern[0])
         {
         if ( ! fQuiet)
            {
            bprint(bp, lad.fNoDelete ? TEXT("NoDelete ") : TEXT("Deleting "));
            bprint(bp, pszPath);
            }
         if ( ! lad.fNoDelete &&  ! DeleteFile(pszPath))
            {
            bprint_EndLine(bp);
            err = GetLastError();
            ReportError(err, "DeleteFile", pszPath);
            if ( ! (fdwFlags & TDT_CONTINUE))
               if ( ! fQuiet) bprintl(bp, "Aborting.");
            }
         }
      }

   bprint_EndLine(bp);
   return err;
}
