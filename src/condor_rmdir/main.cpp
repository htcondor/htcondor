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
#include <windowsx.h>
#include "common.h"

#define HARYLIST_INCLUDE_CODE 1
#include "harylist.h"

#define BPRINT_INCLUDE_CODE 1
#include "bprint.h"

#ifdef UNICODE
#define TokenizeString (LPCTSTR*) CommandLineToArgvW
#else
#include "tokenize.h"
#endif

#include "condor_rmdir.h"

BPRINT_BUFFER g_bprintErr = {0, 0x10000};
BPRINT_BUFFER * g_pbpErr = &g_bprintErr;
BPRINT_BUFFER * g_pbpDiag = &g_bprint;

// tell the linker to include these libraries
//
#pragma comment(lib,"kernel32.lib")
#pragma comment(lib,"user32.lib")
#pragma comment(lib,"advapi32.lib")
#pragma comment(lib,"shlwapi.lib") // for the PathXXX functions

int GetLastErrorText (int err, LPTSTR szErr, int cchErr)
{
   return FormatMessage (FORMAT_MESSAGE_IGNORE_INSERTS
                         | FORMAT_MESSAGE_FROM_SYSTEM,
                         0, err, 0, szErr, cchErr,
                         NULL);
}

int CompareMemory(const char * pb1, const char * pb2, int cb)
{
   if (cb & 3)
      {
      for (int ii = 0; ii < cb; ++ii)
         {
         int diff = pb1[ii] - pb2[ii];
         if (diff)
            return diff;
         }
      }
   else
      {
      const int * pdw1 = (const int *)pb1;
      const int * pdw2 = (const int *)pb2;
      for (int ii = 0; ii < cb/4; ++ii)
         {
         int diff = pdw1[ii] - pdw2[ii];
         if (diff)
            return diff;
         }
      }
   return 0;
}

#ifdef D_ALWAYS
/*+
 *
 * dprintf is used by condor, so we include this so we can host
 * condor(ish) code (like reg.cpp)
 *
 *-===============================================================*/

static int __cdecl dprintf(int flags, LPCTSTR pszFormat, ...) 
{ 
   if (!(DebugFlags & flags))
       return 0;

   va_list va;
   va_start(va, pszFormat);
   int cch = bvprintf(g_bprint, pszFormat, va);
   va_end(va);

   // if this string ended in \n, flush the bprint buffer.
   if (g_bprint.cch > 0 && g_bprint.psz[g_bprint.cch-1] == _UT('\n'))
       bprint_Write();

   return cch;
}
#endif

void ReportError(int err, LPCSTR szContext, LPCTSTR pszPath)
{
   BPRINT_BUFFER & bp = *g_pbpErr;

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

typedef struct _OPENLIST_FILE {

   INT             eFileType;               // app specific type. not used by explorer
   LPARAM          lParam;                  // explorer uses this for the PEXPITEMLIST pointer
   DWORD           dwAttributes;            // explorer attributes for the file

   HRESULT         hr;

   union {

      DWORD        fdwOpen;

      struct {

         DWORD     fGroup              : 1;
         DWORD     fFirstInGroup       : 1;
         DWORD     fLastInGroup        : 1;
         DWORD     fReserved           : 13;
         DWORD     fUser               : 16; // user flags (openlist does not look at them).

         };

      };

   TCHAR           szPathName[2];

} OPENLIST_FILE, *POPENLIST_FILE;

// an HOPENLIST is an array of pointers to OPENLIST_FILES
// we can use the PtrList_xxx functions to manipulate 
// this list. see harylist.h for details.
//
typedef HaryList<POPENLIST_FILE> * HOPENLIST;

static struct _app_globals {


   HINSTANCE hinst;
   DWORD     tidUI;

   union {
      DWORD fdwFlags;
      struct {
         DWORD fNoLogo                     : 1;
         DWORD fShowLogo                   : 1;
         DWORD fNoUI                       : 1;
         DWORD fLogoCmd                    : 1;
         DWORD fExitWhenDone               : 1;
         DWORD fUseage                     : 1;
         DWORD fVerbose                    : 1;
         DWORD fDiagnostic                 : 1;
         DWORD fPassOnExceptions           : 1;
         DWORD fExpectCopyOnWriteException : 1;
         DWORD fProgress                   : 1; // printf is in progress mode.
         DWORD fCancel                     : 1; // cancel for non-progman processing.
         };
      };

   HOPENLIST hlstOpen;

   // these are set during command line parsing and used
   // by subsequent commands.
   BOOL      fSubDirs;
   BOOL      fContinue;
   BOOL      fQuiet;
   DWORD     fNoDelete;

   } g_app = {0};


/*+
 *
 *
 *
 *-===============================================================*/

typedef struct _cmdswitch {
   UINT    idCmd;             // command id.
   DWORD   fdwFlags;          // one or more of CMDF_XXX flags
   UINT    cParams;           // number of required parameters.
   LPCTSTR pszParams;         // comma separated list of default params (in table),
                              // or ptr to command line where params start
   } CMDSWITCH;

// specialize the HaryList structure so that we can easly create 
// dynamicly sized arrays (Vectors) of CMDSWITCHes
typedef HaryList<CMDSWITCH> * HCMDLIST;

typedef const struct _cmdtable {
   LPCTSTR   pszName;
   CMDSWITCH cmd;             // command and default params (if any)
   LPCTSTR   pszArgType;      // used for useage
   LPCTSTR   pszUseage;       // used for useage
   } CMDTABLE, * PCMDTABLE;

#define CMDF_GLOBAL           1 // global option. 
#define CMDF_FILEOP           2 // Cmd operates on files previously listed on the commandline (FILEOPS create sections)
#define CMDF_OPT              4 // Cmd is an option for all fileops in it's section. 

BOOL App_LookupCmdLineArg (
   CMDTABLE    aCmdTable[],
   LPCTSTR     pszArg,
   CMDSWITCH * pcmd)
{
   for (LONG ii = 0; NULL != aCmdTable[ii].pszName; ++ii)
      {
      if (StrIsEqualI(pszArg, aCmdTable[ii].pszName))
         {
         *pcmd = aCmdTable[ii].cmd;
         return TRUE;
         }
      }

   return FALSE;
}

/*+
 *
 *-===============================================================*/

#define APP_CMD_SEPARATOR       0

#define APP_CMD_NOGUI           1  // don't bring up the main window.
#define APP_CMD_NOLOGO          2  // don't bring up the splash
#define APP_CMD_LOGO            3  // force the logo to come up.
#define APP_CMD_DEMO            4  // force demo mode
#define APP_CMD_ARGFILE         5  // pull command line arguments from a file.
#define APP_CMD_EXIT            6
#define APP_CMD_USEAGE          7
#define APP_CMD_VERBOSE         8
#define APP_CMD_DIAGNOSTIC      9
#define APP_CMD_PATH           10
#define APP_CMD_SUBDIRS        11
#define APP_CMD_CONTINUE       12
#define APP_CMD_QUIET          13
#define APP_CMD_NODELETE       14


#define APP_CMD_BREAK_IN_DEBUG 666


// NOTE: do commands get translated???
//
static const CMDTABLE g_aAppCommands[] = {

// _UT("NOLOGO"),       APP_CMD_NOLOGO,       CMDF_GLOBAL, 0, NULL,     NULL,          _UT(""),
// _UT("LOGO"),         APP_CMD_LOGO,         CMDF_GLOBAL, 0, NULL,     NULL,          _UT(""),
   _UT("HELP"),         APP_CMD_USEAGE,       CMDF_GLOBAL, 0, NULL,     NULL,          _UT("Print useage"),
   _UT("?"),            APP_CMD_USEAGE,       CMDF_GLOBAL, 0, NULL,     NULL,          _UT("Print useage"),
   _UT("VERBOSE"),      APP_CMD_VERBOSE,      CMDF_GLOBAL, 0, NULL,     NULL,          _UT("Print detailed output."),
   _UT("DIAGNOSTIC"),   APP_CMD_DIAGNOSTIC,   CMDF_GLOBAL, 0, NULL,     NULL,          _UT("Print out internal flow of control information."),
   _UT("PATH"),         APP_CMD_PATH,         CMDF_GLOBAL, 1, NULL,     _UT("path"),   _UT("delete <path>"),
   _UT("S"),            APP_CMD_SUBDIRS,      CMDF_OPT,    0, NULL,     NULL,          _UT("include subdirectores"),
   _UT("C"),            APP_CMD_CONTINUE,     CMDF_OPT,    0, NULL,     NULL,          _UT("continue on access denied"),
   _UT("Q"),            APP_CMD_QUIET,        CMDF_OPT,	   0, NULL,     NULL,          _UT("Quiet mode - Print error output only."),
   _UT("NODEL"),        APP_CMD_NODELETE,     CMDF_OPT,    0, NULL,     NULL,          _UT("don't delete. (ACLs may still be changed)"),


  #ifdef DEBUG
   _UT("BREAK"),        APP_CMD_BREAK_IN_DEBUG,CMDF_GLOBAL,0, NULL,     NULL,             _UT("INLINE_BREAK during command line parsing."),
  #endif
   NULL, 0, 0, 0, NULL, NULL, NULL, // end of table
   };

/*+
 * cook the tokenized arg list into a HLST of commands.
 *
 *-===============================================================*/

HRESULT App_CookArgList (
   HCMDLIST * phlst,
   LPCTSTR    aArgs[],
   UINT       ixFirst,  // first arg to execute
   UINT       cArgs)    // count of args to execute
{
   HRESULT    hr = S_FALSE; // there were no args...
   TCHAR      szTempArg[64]; // temp if we need to copy and arg

   HCMDLIST   hlst;

   *phlst = NULL;
   hr = Vector_Create(&hlst, cArgs, -1);
   if (FAILED(hr))
      return hr;

   // look at args, parsing switches and building up the array of filename pointers
   //
   UINT ixLast = (ixFirst + cArgs);
   for (UINT ii = ixFirst; ii < ixLast; ++ii)
      {
      LPCTSTR pszArg = aArgs[ii];
      if ( ! pszArg)
         break;

      // assume a file open command.
      //
      hr = S_OK;
      CMDSWITCH cmd = {APP_CMD_PATH, 0, 1, NULL};

      // if the first character of the arg is an '@', what follows should be a command file.
      //
      if (pszArg[0] == '@')
         {
         // command will be an argfile command
         //
         cmd.idCmd     = APP_CMD_ARGFILE;
         cmd.cParams   = 1;    // on arg needed
         cmd.pszParams = NULL; // no default

         // if next character is not a 0, it must be the filename
         // otherwise, suck up the next token and use it as the filename.
         //
         pszArg = StrCharNext (pszArg); // skip '@'
         if (0 != pszArg[0])
            {
            cmd.pszParams = pszArg;
            }
         else if (ii+1 < ixLast) // next pszArg belongs to us..
            {
            LPCTSTR pszT = aArgs[ii+1];
            if ('-' != pszT[0] && '/' != pszT[0])
               {
               cmd.pszParams = pszT;
               ++ii;  // advance the loop counter.
               }
            }
         }
      else if ('-' == pszArg[0] || '/' == pszArg[0])
         {
         pszArg = StrCharNext (pszArg);
         if (0 == pszArg[0])
            {
            hr = E_INVALIDARG;
            goto bail;
            }

         // look for a ':' in the arg, and
         //
         LPCTSTR psz = StrCharNext(pszArg);
         LPCTSTR pszParam = NULL;
         while (0 != psz[0])
            {
            // if the arg contains a colon, set pszParam to point to it
            // and extract the stuff before the colon as the actual arg.
            //
            if (':' == psz[0])
               {
               pszParam = StrCharNext (psz);
               UINT cch = (UINT)(((LPARAM)psz - (LPARAM)pszArg) / NUMBYTES(TCHAR));
               StrCopyN (szTempArg, NUMCHARS(szTempArg), pszArg, cch + 1);
               DASSERT(0 == szTempArg[min(NUMCHARS(szTempArg)-1, cch)]);
               pszArg = szTempArg;
               break;
               }

            psz = StrCharNext(psz);
            }

         // lookup the argment
         //
         if ( ! App_LookupCmdLineArg (g_aAppCommands, pszArg, &cmd))
            {
            bprintfl("%s is not a valid argument", pszArg);
            hr = E_INVALIDARG;
            goto bail;
            }

         if (pszParam)
            {
            if (cmd.cParams < 1)
               {
               // if we have a param, but the arg doesn't take any, bail.
               //
               bprintfl("the %s argument is does not take parameters", pszArg);
               hr = E_INVALIDARG;
               goto bail;
               }

            cmd.pszParams = pszParam;
            }
         else
            {
            // if the command needs args, but none have been found so
            // far, try sucking up the next token in the command line
            //
            if (cmd.cParams > 0 && NULL == cmd.pszParams)
               {
               if (ii+1 < ixLast) // next pszArg belongs to us..
                  {
                  LPCTSTR pszT = aArgs[ii+1];
                  if ('-' != pszT[0] && '/' != pszT[0])
                     {
                     cmd.pszParams = pszT;
                     ++ii;  // advance the loop counter.
                     }
                  }
               }
            }
         }
      else
         {
         // not a switch, this is an implied file-open command
         //
         cmd.pszParams = pszArg;
         }

      // if the command needs an arg, but we dont have one.
      // the command line is in error.
      //
      if ((cmd.cParams > 0) && (NULL == cmd.pszParams))
         {
         bprintfl("the %s argument requires parameters", pszArg);
         g_app.fUseage = true;
         hr = E_INVALIDARG;
         goto bail;
         }

      // append the command.
      //
      Vector_AppendItem(hlst, cmd);
      }

   // return the command list
   //
   *phlst = hlst;

bail:
   if (FAILED(hr) || Vector_GetCountSafe(hlst) < 1)
      {
      *phlst = NULL;
      if (hlst)
         Vector_Delete (hlst);
      }
   return hr;
}

/*+
 *
 *-===============================================================*/


HRESULT OpenList_Add (
   HOPENLIST * phlst,
   LPCTSTR     pszPath,
   DWORD       fdwOpen,
   LPARAM      lParam)
{
   HRESULT hr = S_OK;

   hr = PtrList_CreateIfNot(phlst, 16, 16);
   if (FAILED(hr))
      return hr;

   int cbPath = StrAllocBytes(pszPath);
   int cbItem = NUMBYTES(OPENLIST_FILE) + cbPath;

   POPENLIST_FILE polf = PtrList_AllocItem(*phlst, cbItem);
   if ( ! polf)
      return E_OUTOFMEMORY;

   ZeroStruct(polf);
   CopyMemory(polf->szPathName, pszPath, cbPath);
   polf->fdwOpen = fdwOpen & 0xFFFF0000;
   polf->lParam  = lParam;

   hr = PtrList_AppendItem(*phlst, polf);
   return hr;
}

static HRESULT CALLBACK OpenList_Open (
   POPENLIST_FILE polf,
   LPARAM         lArgs)
{
   HRESULT hr = S_OK;

   if (E_CANCEL != polf->hr)  // Possible if a queue'd open was canceled on progman and we are being called for possible batch undo handling
      {
      hr = E_FILE_UNSUPPORTED;

      if (g_app.fDiagnostic)
         bprintfl("\nDeletePath \"%s\" recursive = %d", polf->szPathName, g_app.fSubDirs);

      DWORD fdwTraverse = 0;
      if (g_app.fSubDirs)     fdwTraverse |= TDT_SUBDIRS;
      //if (g_app.fNoFiles)   fdwTraverse |= TDT_NOFILES;
      //if (g_app.fNoDirs)    fdwTraverse |= TDT_NODIRS;
      if (g_app.fContinue)    fdwTraverse |= TDT_CONTINUE;
      if (g_app.fDiagnostic)  fdwTraverse |= TDT_DIAGNOSTIC;

      DWORD fdwSecurity = 0;
      if (g_app.fNoDelete) fdwSecurity = DP_S_NODELETE;

      DWORD fdwVerbose = 0;
      if (g_app.fVerbose)     fdwVerbose |= DP_V_VERBOSE;
      if (g_app.fDiagnostic)  fdwVerbose |= DP_V_DIAGNOSTIC;
      if (g_app.fQuiet)       fdwVerbose |= DP_V_QUIET;

      int err = DeletePath(polf->szPathName, TEXT(""), fdwTraverse, fdwSecurity, fdwVerbose, g_bprint);

      //  DeletePath has already reported errors.
      /*
      if (err && ERROR_NO_MORE_FILES != err)
         {
         TCHAR szErr[MAX_PATH];
         if (GetLastErrorText(err, szErr, NUMCHARS(szErr)) <= 0)
            wsprintf(szErr, TEXT("Error %d"), err);
         bprintfl ("%s\n", szErr);
         }
      */
      if (g_app.fContinue)
         hr = S_OK;
      else if (err)
         hr = E_CANCEL;
      }

   return hr;
}

HRESULT OpenList_OpenFiles (
   HOPENLIST hlst,
   LPARAM    lArgs)
{
   HRESULT  hr = S_FALSE;
   LONG cFiles = PtrList_GetCountSafe(hlst);

   for (LONG ii = 0; ii < cFiles; ii++)
      {
      POPENLIST_FILE polf = PtrList_GetItem(hlst, ii);
      if (cFiles > 1)
         {
         polf->fGroup = TRUE;
         if (ii == 0)
            polf->fFirstInGroup = TRUE;
         if (ii == (cFiles-1))
            polf->fLastInGroup = TRUE;
         }

      hr = OpenList_Open(polf, lArgs);
      }

   return hr;
}

HRESULT OpenList_Destroy (
   HOPENLIST hlst)
{
   if (hlst)
      {
      PtrList_Delete(hlst);
      }

   return S_OK;
}



static DWORD MergeCmdFlag(LPCTSTR pszParams, DWORD fdwFlag, DWORD fdwOut)
{
   bool fFlag = (pszParams[0] == 0) || (pszParams[0] == 'T') || (pszParams[0] == 't') || (pszParams[0] == '1');
   if (fFlag)
      fdwOut |= fdwFlag;
   else
      fdwOut &= ~fdwFlag;
   return fdwOut;
}


static HRESULT App_ExecuteCmds (
   HCMDLIST  hlstCmds)
{
   BOOL      fHasFileOp = FALSE;
   HRESULT   hr = S_FALSE; //

   //
   // this get filled in by various commands, and used in arguments in other commands.
   //
   BPRINT_BUFFER & bp = g_bprint;

   // execute open and general commands from the HLST of commands.
   //
   LONG ii;
   for (ii = 0; ii < Vector_GetCount (hlstCmds); ++ii)
      {
      CMDSWITCH * pcmd = Vector_GetItemPtr(hlstCmds, ii);
      switch (pcmd->idCmd)
         {
         //case APP_CMD_NOGUI:
         //   g_app.fNoUI = TRUE;
         //   break;

         case APP_CMD_NOLOGO:
            hr = S_FALSE;    // allow last session to open
            g_app.fShowLogo = FALSE;
            g_app.fLogoCmd = TRUE;
            break;

         case APP_CMD_LOGO:
            hr = S_FALSE;   // allow last session to open
            g_app.fShowLogo = TRUE;
            g_app.fLogoCmd = TRUE;
            break;

         case APP_CMD_EXIT:
            g_app.fExitWhenDone = TRUE;
            break;

         case APP_CMD_USEAGE:
            g_app.fUseage = TRUE;
            break;

         case APP_CMD_VERBOSE:
            g_app.fVerbose = TRUE;
            break;

         case APP_CMD_DIAGNOSTIC:
            g_app.fDiagnostic = TRUE;
            break;

         case APP_CMD_ARGFILE:
            hr = E_NOTIMPL; //App_ExecuteCommandFile(pcmd->pszParams);
            break;

         case APP_CMD_PATH:
            {
            LPARAM lParam = 0;
            hr = OpenList_Add(&g_app.hlstOpen,pcmd->pszParams,0,lParam);
            if (FAILED(hr))
               break;
            }
            break;

         case APP_CMD_SUBDIRS:
            g_app.fSubDirs = TRUE;
            break;

         case APP_CMD_CONTINUE:
            g_app.fContinue = TRUE;
            break;

         case APP_CMD_NODELETE:
            g_app.fNoDelete = TRUE;
            break;

         case APP_CMD_QUIET:
            g_app.fQuiet = TRUE;
            break;

         case APP_CMD_BREAK_IN_DEBUG:
            INLINE_BREAK;
            break;

         default:
            if (pcmd->fdwFlags & CMDF_FILEOP)
               fHasFileOp = TRUE;
            break;
         }
      }

   // next execute commands that will edit the open project
   //
   if (g_app.hlstOpen)
      {
      hr = PtrList_Iterate(g_app.hlstOpen, OpenList_Open, 0);
      if (g_app.hlstOpen)
         OpenList_Destroy (g_app.hlstOpen);
      }

   return hr;
}

/*+
 *
 *-===============================================================*/

HRESULT App_ExecuteArgList (
   LPCTSTR   aArgs[],
   UINT      ixFirst,  // first arg to execute
   UINT      cArgs)    // count of args to execute
{
   // turn the tokenized arg list into a HCMDLIST of commands.
   //
   HCMDLIST  hlstCmds = NULL;
   HRESULT hr = App_CookArgList (&hlstCmds, aArgs, ixFirst, cArgs);
   if (FAILED(hr) || ! hlstCmds)
      return hr;

   // execute the HCMDLIST
   //
   hr = App_ExecuteCmds(hlstCmds);

   // and now free the HCMDLIST
   //
   Vector_Delete (hlstCmds);

   return hr;
}


/*+
 *
 *-===============================================================*/


HRESULT App_ExecuteCommandLine (
   LPCTSTR pszCmdLine,
   BOOL    fFirstArgIsExe)
{
   HRESULT   hr = S_FALSE; // there were no args...

   int cArgs = 0;
   LPCTSTR * aArgs = TokenizeString (pszCmdLine, &cArgs);
   if (aArgs)
      {
      LPCTSTR pszArg = aArgs[0];
      if (*pszArg == '@' || *pszArg == '-' || *pszArg == '/')
         fFirstArgIsExe = FALSE;

      int ixFirst = fFirstArgIsExe ? 1 : 0;
      if (cArgs > ixFirst)
         {
         hr = App_ExecuteArgList (aArgs, ixFirst, cArgs - ixFirst);
         }

      HeapFree(GetProcessHeap(), 0, aArgs);
      }

   return hr;
}

const TCHAR * _pszModuleName; // global module name
const TCHAR * _pszModulePath; // global path name

// allocate memory from the heap and read in the module filename, then split it
// into path and filename and store the results as global pointers. note
// that this allocation will NEVER be freed. 
//
void App_SetModuleInfo(void)
{
   TCHAR * pszBase =(TCHAR*)malloc(sizeof(TCHAR) * (MAX_PATH+3));
   if ( ! pszBase)
      {
	  _pszModulePath = _pszModuleName = TEXT("");
	  return;
      }

   TCHAR * psz = pszBase;
   *psz++ = 0; // so we have room for a "" path if the module filename has no path
   int cch = GetModuleFileName(NULL, psz, MAX_PATH+1);

   _pszModuleName = psz;
   _pszModulePath = psz;

   // if module path starts with N:, then skip over that and any \ that
   // follow
   if ((psz[0] & ~0x20) >= 'A' && (psz[0] & ~0x20) <= 'Z' && psz[1] == ':')
      {
      psz += 2;
      while (psz[0] == '\\')
         ++psz;
      _pszModuleName = psz;
      }

   // scan for path separators (\) and set the name to point to
   // the first character after it. when this loop is done, this
   // will result in name pointing to the first character after
   // the last path separator.
   //
   while (*psz)
      {
      if (*psz == '\\')
        _pszModuleName = psz+1;
      psz = CharNext(psz);
      }

   // if the name still points to the start of the string, there is no path.
   // so point _pszModulePath at the \0 that we reserved at the beginning
   // of the allocation. 
   //
   if (_pszModuleName == _pszModulePath)
      _pszModulePath = pszBase;
   else
      {
      if (_pszModuleName-2 >= _pszModulePath)
         {
         // we need to be careful not to delete \ folling a drive letter,
         // if that happens, we have to move the name by 1 character
         // so there is room for a null terminator after N:\ and before
         // the name.
         if (_pszModuleName[-2] == ':' &&
             _pszModuleName[-1] == '\\')
            {
            RtlMoveMemory((void*)(_pszModuleName+1), (void*)(_pszModuleName),
                          sizeof(TCHAR) *((cch+1) - (_pszModuleName - _pszModulePath)));
            _pszModuleName += 1;
            }
         ((TCHAR *)_pszModuleName)[-1] = 0;
         }
      }
}

HRESULT App_Early_Initialize()
{
   HRESULT hr = S_OK;

   bprint_Initialize();
   HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);
   if (hErr != g_bprint.hOut || hErr != NULL || hErr != INVALID_HANDLE_VALUE)
      {
      bprint_Initialize(g_bprintErr); 
      g_bprintErr.hOut = hErr;
      g_bprintErr.CodePage = CP_OEMCP;
      g_pbpErr = &g_bprintErr;
      }

   // this function initializes the global module name and path name
   //
   App_SetModuleInfo();

   return hr;
}


void App_Cleanup(bool fFlush)
{
   bprint_Terminate(fFlush);
   if (g_bprintErr.hOut)
      {
      if (fFlush)
         bprint_EndLine(g_bprintErr);
      if (g_bprintErr.hOut != GetStdHandle(STD_ERROR_HANDLE))
         CloseHandle(g_bprintErr.hOut);
      g_bprintErr.hOut = NULL;
      }
   bprint_Terminate(g_bprintErr, false);
}

int __cdecl main()
{
   HRESULT hr = App_Early_Initialize();
   if (SUCCEEDED(hr))
      {
      hr = App_ExecuteCommandLine (GetCommandLine(), TRUE);
      if (FAILED(hr))
         g_app.fUseage = TRUE;
      }

   if (g_app.fShowLogo || g_app.fUseage)
      {
      LPCTSTR pszAppName = _pszModuleName;
      bprintfl("%s version 1.0.0 Built " __DATE__ __TIME__, pszAppName);
      }

   if (g_app.fUseage)
      {
      LPCTSTR pszAppName = _pszModuleName;

      bprintf("\nuseage: %s [/? | /HELP] [@{argfile}] [args] <dir>\n\n", pszAppName);
      bprintf("  remove directory <dir>, taking ownership and removing\n");
      bprintf("  ACLs as necessary. Where [args] is one or more of\n\n");

      for (int ii = 0; ii < NUMELMS(g_aAppCommands); ++ii)
         {
         CMDTABLE * pCmd = &g_aAppCommands[ii];
         if ( ! pCmd->pszName)
            break;
         LPCTSTR pszCmd = pCmd->pszName;
         TCHAR szCmd[128];
         if (pCmd->cmd.cParams > 0)
            {
            StrCopy(szCmd, pCmd->pszName, NUMCHARS(szCmd));
            int cch = StrLen(szCmd);
            szCmd[cch++] = ':';
            StrCopy(szCmd+cch, pCmd->pszArgType, NUMCHARS(szCmd)-cch);
            pszCmd = szCmd;
            }
         bprintf("  /%-16s", pszCmd);
         bprintf("  %s\n", pCmd->pszUseage);
         }
      }

   App_Cleanup(SUCCEEDED(hr));
   return (int)hr;
}
