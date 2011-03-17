//==========================================
// LIBCTINY - Matt Pietrek 2001
// MSDN Magazine, January 2001
//==========================================
#define UNICODE
#include <windows.h>
#include "argcargv.h"

const TCHAR ** _ppszArgv;
const TCHAR ** _SetArgv(const TCHAR ** ppsz)
{
   const TCHAR ** ppszOld = _ppszArgv;
   _ppszArgv = ppsz;
   return ppszOld;
}

#if 1
#undef MoveMemory
#undef RtlMoveMemory
extern "C" __declspec(dllimport) void* __stdcall RtlMoveMemory(void *, const void *, int);

const TCHAR * _pszModuleName;
const TCHAR * _pszModulePath;
void __cdecl _SetModuleInfo( void )
{
   TCHAR * psz =(TCHAR*)HeapAlloc(GetProcessHeap(), 0, sizeof(TCHAR) * (MAX_PATH+2));
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
   //
   if (_pszModuleName == _pszModulePath)
      _pszModulePath = TEXT("");
   else
   {
      if (_pszModuleName-1 > _pszModulePath)
      {
         // we need to be careful not to delete \ folling a drive letter,
         // if that happens, we have to move the name by 1 character
         // so there is room for a null terminator after N:\ and before
         // the name.
         if (_pszModuleName[-1] == ':' &&
             _pszModuleName[-1] == '\\')
         {
            RtlMoveMemory((void*)(_pszModuleName+1), (void*)(_pszModuleName),
                          sizeof(TCHAR) *((cch+1) - (_pszModuleName - _pszModulePath)));
            _pszModuleName += 1;
         }
         //else
         {
            ((TCHAR *)_pszModuleName)[-1] = 0;
         }
      }
   }
}
#else
#define _MAX_CMD_LINE_ARGS  128
int __cdecl _ConvertCommandLineToArgcArgv( void )
{
    int cbCmdLine;
    int argc;
    PSTR pszSysCmdLine, pszCmdLine;

    char **ppszArgv = (char**)HeapAlloc(GetProcessHeap(), 0, sizeof(char*) * _MAX_CMD_LINE_ARGS);
    _ppszArgv = (const char **)ppszArgv;

    // Set to no argv elements, in case we have to bail out
    ppszArgv[0] = 0;

    // First get a pointer to the system's version of the command line, and
    // figure out how long it is.
    pszSysCmdLine = GetCommandLine();
    cbCmdLine = lstrlen( pszSysCmdLine );

    // Allocate memory to store a copy of the command line.  We'll modify
    // this copy, rather than the original command line.  Yes, this memory
    // currently doesn't explicitly get freed, but it goes away when the
    // process terminates.
    pszCmdLine = (PSTR)HeapAlloc( GetProcessHeap(), 0, cbCmdLine+1 );
    if ( !pszCmdLine )
        return 0;

    // Copy the system version of the command line into our copy
    lstrcpy( pszCmdLine, pszSysCmdLine );

    if ( '"' == *pszCmdLine )   // If command line starts with a quote ("),
    {                           // it's a quoted filename.  Skip to next quote.
        pszCmdLine++;

        ppszArgv[0] = pszCmdLine;  // argv[0] == executable name

        while ( *pszCmdLine && (*pszCmdLine != '"') )
            pszCmdLine++;

        if ( *pszCmdLine )      // Did we see a non-NULL ending?
            *pszCmdLine++ = 0;  // Null terminate and advance to next char
        else
            return 0;           // Oops!  We didn't see the end quote
    }
    else    // A regular (non-quoted) filename
    {
        ppszArgv[0] = pszCmdLine;  // argv[0] == executable name

        while ( *pszCmdLine && (' ' != *pszCmdLine) && ('\t' != *pszCmdLine) )
            pszCmdLine++;

        if ( *pszCmdLine )
            *pszCmdLine++ = 0;  // Null terminate and advance to next char
    }

    // Done processing argv[0] (i.e., the executable name).  Now do th
    // actual arguments

    argc = 1;

    #if 0
    while ( 1 )
    {
        // Skip over any whitespace
        while ( *pszCmdLine && (' ' == *pszCmdLine) || ('\t' == *pszCmdLine) )
            pszCmdLine++;

        if ( 0 == *pszCmdLine ) // End of command line???
            return argc;

        if ( '"' == *pszCmdLine )   // Argument starting with a quote???
        {
            pszCmdLine++;   // Advance past quote character

            ppszArgv[ argc++ ] = pszCmdLine;
            ppszArgv[ argc ] = 0;

            // Scan to end quote, or NULL terminator
            while ( *pszCmdLine && (*pszCmdLine != '"') )
                pszCmdLine++;

            if ( 0 == *pszCmdLine )
                return argc;

            if ( *pszCmdLine )
                *pszCmdLine++ = 0;  // Null terminate and advance to next char
        }
        else                        // Non-quoted argument
        {
            ppszArgv[ argc++ ] = pszCmdLine;
            ppszArgv[ argc ] = 0;

            // Skip till whitespace or NULL terminator
            while ( *pszCmdLine && (' '!=*pszCmdLine) && ('\t'!=*pszCmdLine) )
                pszCmdLine++;

            if ( 0 == *pszCmdLine )
                return argc;

            if ( *pszCmdLine )
                *pszCmdLine++ = 0;  // Null terminate and advance to next char
        }

        if ( argc >= (_MAX_CMD_LINE_ARGS) )
            return argc;
    }
    #endif
}
#endif
