#define UNICODE
#include <windows.h>
#include <windowsx.h>
#include "common.h"

#define HARYLIST_INCLUDE_CODE 1
#include "harylist.h"

#define BPRINT_INCLUDE_CODE 1
#include "bprint.h"

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

extern "C" {
   void* _cdecl memset(void * ptr, int val, size_t cb) {
   byte * p = (byte*)ptr;
   while (cb > 0) {
      --cb;
      *p++ = val;
      }
   return ptr;
   }
   void * _cdecl memcpy(void * dst, const void * src, size_t cb) {
      byte * d = (byte*)dst;
      const byte * s = (const byte*)src;
      while (cb > 0) {
        --cb;
        *d++ = *s++;
      }
   return dst;
   }
};

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

//#define OPT_F_SUBDIRS  0x0001

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
         DWORD     fFileInfo           : 1;  // FileIO info is filled in.
         DWORD     fReserved           : 7;
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

#ifdef UNICODE
#define TokenizeString _TokenizeStringW
extern LPCTSTR * _TokenizeStringW (LPCTSTR pszInput, LPUINT  pcArgs);
#else
#define TokenizeString _TokenizeStringA
extern LPCTSTR * _TokenizeStringA (LPCTSTR pszInput, LPUINT  pcArgs);
#endif

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

BOOL SfApp_LookupCmdLineArg (
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

#define APP_CMD_SEGMENT         0  // MUST BE 0 !!

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

HRESULT SfApp_CookArgList (
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
         if ( ! SfApp_LookupCmdLineArg (g_aAppCommands, pszArg, &cmd))
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
 *  Break command line into segments.  so that renders get grouped
 *  with opens that come before them.  basically, once we see a render
 *  we treat the next open as a 'start of segment'
 *
 *-===============================================================*/

static HRESULT SfApp_SegmentCmds (
   HCMDLIST hlstCmds)
{
   BOOL fSegmentNextOpen = FALSE;

   LONG cCmds = Vector_GetCount (hlstCmds);
   for (LONG ii = 0; ii < cCmds; ++ii)
      {
      CMDSWITCH * pcmd = Vector_GetItemPtr(hlstCmds, ii);
      if (APP_CMD_SEGMENT == pcmd->idCmd)
         {
         return E_UNEXPECTED;
         }

      if (APP_CMD_PATH == pcmd->idCmd)
         {
         if (fSegmentNextOpen)
            {
            // Insert a segment marker. 
            DASSERT(APP_CMD_SEGMENT == 0);
            static const CMDSWITCH cmd = {0};
            Vector_InsertItem<CMDSWITCH> (hlstCmds, ii, cmd);
            cCmds++; // Increment command count (to account for Segment marker)
            }
         fSegmentNextOpen = FALSE;
         }
      else if (pcmd->fdwFlags & CMDF_FILEOP)
         {
         fSegmentNextOpen = TRUE;
         }
      }

   return S_OK;
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

   SfZeroStruct(polf);
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


HRESULT SfApp_ExecuteCommandFile (LPCTSTR pszCommandFile); // Forward ref

static DWORD MergeCmdFlag(LPCTSTR pszParams, DWORD fdwFlag, DWORD fdwOut)
{
   bool fFlag = (pszParams[0] == 0) || (pszParams[0] == 'T') || (pszParams[0] == 't') || (pszParams[0] == '1');
   if (fFlag)
      fdwOut |= fdwFlag;
   else
      fdwOut &= ~fdwFlag;
   return fdwOut;
}


static HRESULT SfApp_ExecuteCmdSegment (
   HCMDLIST  hlstCmds,
   LONG   *  pixSeg)
{
   BOOL      fHasFileOp = FALSE;
   HRESULT   hr = S_FALSE; //

   //
   // this get filled in by various commands, and used in arguments in other commands.
   //
   BPRINT_BUFFER & bp = g_bprint;

   // execute open and general commands from the HLST of commands.
   //
   LONG ixSegStart = *pixSeg;
   LONG ixSegEnd = Vector_GetCount (hlstCmds);
   LONG ii;
   for (ii = ixSegStart; ii < ixSegEnd; ++ii)
      {
      CMDSWITCH * pcmd = Vector_GetItemPtr(hlstCmds, ii);
      switch (pcmd->idCmd)
         {
         case APP_CMD_SEGMENT:
            ixSegEnd = ii+1;
            break;

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
            hr = SfApp_ExecuteCommandFile(pcmd->pszParams);
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

   *pixSeg = ixSegEnd;
   return hr;
}

/*+
 *
 *-===============================================================*/

HRESULT SfApp_ExecuteArgList (
   LPCTSTR   aArgs[],
   UINT      ixFirst,  // first arg to execute
   UINT      cArgs)    // count of args to execute
{
   HCMDLIST  hlstCmds = NULL;
   HRESULT   hr;
   //BOOL      fLoadedMedia = FALSE;

   // turn the tokenized arg list into a HLST of commands.
   //
   hr = SfApp_CookArgList (&hlstCmds, aArgs, ixFirst, cArgs);
   if (FAILED(hr) || ! hlstCmds)
      return hr;

   // segment the argument list. this adds APP_CMD_SEGMENT entries
   // between renders and opens
   //
   SfApp_SegmentCmds(hlstCmds);

   // execute the command list, one segment at a time.
   //
   LONG cCmds = Vector_GetCount (hlstCmds);
   for (LONG ix = 0; IS_WITHIN_RGN(ix,0,cCmds); /* ix increment is implied.. */)
      {
      LONG ixNext = ix + 1;

      hr = SfApp_ExecuteCmdSegment(hlstCmds, &ix);
      if (FAILED(hr))
         break;

      if (ix < ixNext)
         {
         hr = E_UNEXPECTED;
         break;
         }
      }

   // we can free the command list now.
   //
   Vector_Delete (hlstCmds);

   return hr;
}

/*+
 *
 *-===============================================================*/

HRESULT SfApp_ExecuteCommandFile (
   LPCTSTR pszCommandFile)
{
   if ( ! pszCommandFile)
      {
      g_app.fUseage = ! g_app.fNoLogo;
      bprintfl("Error: no command file specified.");
      return E_INVALIDARG;
      }

   HANDLE hFile;
   hFile = CreateFile(pszCommandFile, GENERIC_READ, FILE_SHARE_READ, NULL,
                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

   if ( ! hFile || INVALID_HANDLE_VALUE == hFile)
      {
      g_app.fUseage = ! g_app.fNoLogo;
      bprintfl("Error: could not open command file \"%s\"", pszCommandFile);
      return E_FILEOPEN;
      }

   DWORD   cb = GetFileSize(hFile, NULL);
   LPVOID  pszCommand = GlobalAllocPtr(GPTR, cb + 2);
   if ( ! pszCommand)
      {
      VERIFY(CloseHandle(hFile));
      return E_OUTOFMEMORY;
      }

   LPCTSTR * aArgsFile = NULL;
   UINT      cArgsFile = 0;

   if (ReadFile (hFile, pszCommand, cb, &cb, NULL))
      {
      ((LPBYTE)pszCommand)[cb] = 0;   // null terminate the file.
      ((LPBYTE)pszCommand)[cb+1] = 0; //

      // parse the file as if it were a command line.
      //
      //aArgsFile = SfApp_TokenizeCommandLine ((LPCTSTR)pszCommand, &cArgsFile);
      aArgsFile = TokenizeString ((LPCTSTR)pszCommand, &cArgsFile);
      }
   GlobalFreePtr (pszCommand);
   VERIFY(CloseHandle (hFile));

   HRESULT hr = S_FALSE; // file was empty
   if (aArgsFile)
      {
      // load and parse command file here.
      hr = SfApp_ExecuteArgList (aArgsFile, 0, cArgsFile);
      GlobalFreePtr (aArgsFile);
      }

   return hr;
}

/*+
 *
 *-===============================================================*/


HRESULT SfApp_ExecuteCommandLine (
   LPCTSTR pszCmdLine,
   BOOL    fFirstArgIsExe)
{
   LPCTSTR * aArgs;
   UINT      cArgs;
   HRESULT   hr = S_FALSE; // there were no args...

   //aArgs = SfApp_TokenizeCommandLine (pszCmdLine, &cArgs);
   aArgs = TokenizeString (pszCmdLine, &cArgs);
   if (aArgs)
      {
      LPCTSTR pszArg = aArgs[0];
      if (*pszArg == '@' || *pszArg == '-' || *pszArg == '/')
         fFirstArgIsExe = FALSE;

      UINT ixFirst = fFirstArgIsExe ? 1 : 0;
      if (cArgs > ixFirst)
         {
         hr = SfApp_ExecuteArgList (aArgs, ixFirst, cArgs - ixFirst);
         }
      GlobalFreePtr (aArgs);
      }

   return hr;
}

// these are in libctiny
extern const TCHAR * _pszModuleName; // global module name
extern const TCHAR * _pszModulePath; // global path name
extern void __cdecl _SetModuleInfo( void ); // the function that initializes them.

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
   _SetModuleInfo();

   return hr;
}


void App_Cleanup(bool fFlush)
{
   // CoUninitialize();

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

int __cdecl main(int argc, const char * argv[])
{
   HRESULT hr = App_Early_Initialize();
   if (SUCCEEDED(hr))
      {
      hr = SfApp_ExecuteCommandLine (GetCommandLine(), TRUE);
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
      //echo argv.
      //for (int ii = 0; ii < argc; ++ii)
      //   bprintf("arg[%d] = %s\n", ii, argv[ii]);
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

/*
#pragma comment(linker, "/ENTRY:_tmainCRTStartup")
//#pragma comment(linker, "/OPT:NOWIN98")
//#pragma comment(linker, "/ALIGN:512")
//#pragma comment(linker, "/defaultlib:kernel32.lib")

extern "C" {
extern void __cdecl _tmainCRTStartup( void );
int __cdecl _tmain(int argc, char * argv[]);
}

int __cdecl _tmain(int argc, char * argv[])
*/
