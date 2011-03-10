//==========================================
// LIBCTINY - Matt Pietrek 2001
// MSDN Magazine, January 2001
// FILE: CRT0TCON.CPP
//==========================================
#define UNICODE
#include <windows.h>
#include "argcargv.h"
#include "tokenize.h"
#include "initterm.h"

// Force the linker to include KERNEL32.LIB
#pragma comment(linker, "/defaultlib:kernel32.lib")

// Force 512 byte section alignment in the PE file
//#pragma comment(linker, "/ALIGN:512")
//#pragma comment(linker, "/nodefaultlib:libc.lib")
//#pragma comment(linker, "/nodefaultlib:libcmt.lib")

//extern "C" int __cdecl main();    // In user's code
//extern "C" int __cdecl main(int, char **, char **);    // In user's code
extern "C" int __cdecl main(int, const TCHAR **);    // In user's code

//
// Modified version of the Visual C++ startup code.  Simplified to
// make it easier to read.  Only supports ANSI programs.
//
extern "C" void __cdecl mainCRTStartup( void )
{
    int mainret, cArgs;

    // set up our minimal cheezy atexit table
    _atexit_init();

    // Call C++ constructors
    _initterm( __xc_a, __xc_z );

    // parse the command line and set it as a global variable
    LPCTSTR * ppszArgs = _TokenizeString (GetCommandLine(), (UINT*)&cArgs);
    _SetArgv(ppszArgs);
    mainret = main(cArgs, ppszArgs);

    _DoExit();

    ExitProcess(mainret);
}
