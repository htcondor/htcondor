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
#include <tchar.h>
#include "exception_handling.WINDOWS.h"

#include "condor_debug.h"

//============================== Global Variables =============================
//
// Declare the static variables of the ExceptionHandler class
//
TCHAR ExceptionHandler::m_szLogFileName[MAX_PATH];
LPTOP_LEVEL_EXCEPTION_FILTER ExceptionHandler::m_previousFilter;
HANDLE ExceptionHandler::m_hReportFile;
pid_t ExceptionHandler::m_pid = -1;
ExceptionHandler::SYMINITIALIZEPROC ExceptionHandler::_SymInitialize = 0;
ExceptionHandler::SYMCLEANUPPROC ExceptionHandler::_SymCleanup = 0;
ExceptionHandler::STACKWALKPROC ExceptionHandler::_StackWalk = 0;
ExceptionHandler::SYMFUNCTIONTABLEACCESSPROC
						ExceptionHandler::_SymFunctionTableAccess = 0;
ExceptionHandler::SYMGETMODULEBASEPROC
						ExceptionHandler::_SymGetModuleBase = 0;
ExceptionHandler::SYMGETSYMFROMADDRPROC
						ExceptionHandler::_SymGetSymFromAddr = 0;
ExceptionHandler::SYMGETLINEFROMADDRPROC
						ExceptionHandler::_SymGetLineFromAddr = 0;
ExceptionHandler g_ExceptionHandler;  // Declare global instance of class

//============================== Class Methods =============================

//=============// Constructor //=============
ExceptionHandler::ExceptionHandler( ){
    // Install the unhandled exception filter function
    m_previousFilter = SetUnhandledExceptionFilter(MSJUnhandledExceptionFilter);
    // Figure out what the report file will be named, and store it away
    GetModuleFileName( 0, m_szLogFileName, MAX_PATH );
    // Look for the '.' before the "EXE" extension.  Replace the extension
    // with "RPT"    
	PTSTR pszDot = _tcsrchr( m_szLogFileName, _T('.') );
    if ( pszDot )    {        
		pszDot++;   // Advance past the '.'
        if ( _tcslen(pszDot) >= 3 )
            _tcscpy( pszDot, _T("RPT") );   // "RPT" -> "Report"    
	}
}

//============// Destructor //============
ExceptionHandler::~ExceptionHandler( ){
    SetUnhandledExceptionFilter( m_previousFilter );
}


//===========================================================
// When we are instantiated, our constructor set ourselves
// up to be the exception handler.  Here we provide some methods
// to allow the user to reset the default exception handler on & off.
//==========================================================
void ExceptionHandler::TurnOff() {
	SetUnhandledExceptionFilter( m_previousFilter );
}

void ExceptionHandler::TurnOn() {
	SetUnhandledExceptionFilter(MSJUnhandledExceptionFilter);
}

//==============================================================
// Lets user change the name of the report file to be generated 
//==============================================================
void ExceptionHandler::SetLogFileName( const TCHAR * pszLogFileName ){
    _tcsncpy( m_szLogFileName, pszLogFileName, COUNTOF(m_szLogFileName) );
}

//==============================================================
// Lets user change the pid of the process we are dumping for
//==============================================================
void ExceptionHandler::SetPID( pid_t pid ){
    m_pid = pid;
}

//===========================================================
// Entry point where control comes on an unhandled exception 
//===========================================================
LONG WINAPI ExceptionHandler::MSJUnhandledExceptionFilter(
                                             PEXCEPTION_POINTERS pExceptionInfo )
{    
	dprintf ( D_ALWAYS, "Intercepting an unhandled exception.\n" );	
	m_hReportFile = CreateFile( m_szLogFileName,
                                GENERIC_WRITE,
								0,
                                0,
								CREATE_ALWAYS,
                                FILE_FLAG_WRITE_THROUGH,
                                0 );    
	if ( m_hReportFile )    {
		dprintf ( D_ALWAYS, "Dropping a core file.\n" );
        // SetFilePointer( m_hReportFile, 0, 0, FILE_END );
        GenerateExceptionReport( pExceptionInfo );
        CloseHandle( m_hReportFile );        
		m_hReportFile = 0;    
	}
    if ( m_previousFilter )        
		return m_previousFilter( pExceptionInfo );
    else        
		return EXCEPTION_CONTINUE_SEARCH;
}
//===========================================================================
// Open the report file, and write the desired information to it.  Called by 
// MSJUnhandledExceptionFilter                                               
//===========================================================================
void ExceptionHandler::GenerateExceptionReport(
    PEXCEPTION_POINTERS pExceptionInfo ){    
	// Start out with a banner
    _tprintf( _T("//=====================================================\n") );
    PEXCEPTION_RECORD pExceptionRecord = pExceptionInfo->ExceptionRecord;
	// First print the pid of the process
	_tprintf(   _T("PID: %d\n"), m_pid );
    // Now print information about the type of fault
    _tprintf(   _T("Exception code: %08X %s\n"),
                pExceptionRecord->ExceptionCode,
                GetExceptionString(pExceptionRecord->ExceptionCode) );
    // Now print information about where the fault occured
    TCHAR szFaultingModule[MAX_PATH];    
	DWORD section, offset;
    GetLogicalAddress(  pExceptionRecord->ExceptionAddress,
                        szFaultingModule,
                        sizeof( szFaultingModule ),
                        section, offset );
    _tprintf( _T("Fault address:  %08X %02X:%08X %s\n"),
              pExceptionRecord->ExceptionAddress,
              section, offset, szFaultingModule );
    PCONTEXT pCtx = pExceptionInfo->ContextRecord;    
	// dprintf(D_ALWAYS,"TEST: SHOWING REGISTERS\n");
	// Show the registers
#ifdef _M_IX86  // Intel Only!   
	_tprintf( _T("\nRegisters:\n") );
    _tprintf(_T("EAX:%08X\nEBX:%08X\nECX:%08X\nEDX:%08X\nESI:%08X\nEDI:%08X\n"),
             pCtx->Eax, pCtx->Ebx, pCtx->Ecx, pCtx->Edx, pCtx->Esi, pCtx->Edi );
    _tprintf( _T("CS:EIP:%04X:%08X\n"), pCtx->SegCs, pCtx->Eip );
    _tprintf( _T("SS:ESP:%04X:%08X  EBP:%08X\n"),
              pCtx->SegSs, pCtx->Esp, pCtx->Ebp );
    _tprintf( _T("DS:%04X  ES:%04X  FS:%04X  GS:%04X\n"),
              pCtx->SegDs, pCtx->SegEs, pCtx->SegFs, pCtx->SegGs );
    _tprintf( _T("Flags:%08X\n"), pCtx->EFlags );    
#endif
    if ( !InitImagehlpFunctions() )    {
        //dprintf(D_ALWAYS,"IMAGEHLP.DLL or its exported procs not found\n");
		//dprintf(D_ALWAYS," ----> GetLastError = %d\n",GetLastError() );
#ifdef _M_IX86  // Intel Only!
        // Walk the stack using x86 specific code        
		IntelStackWalk( pCtx );
#endif        
		return;    
	}    
	ImagehlpStackWalk( pCtx );
    _SymCleanup( GetCurrentProcess() );    
	// Just to make sure we didn't get cut off whilst dumping a core
	// file, we print a footer that should allways appear.
	_tprintf( _T("\n//=====================================================\n") );
}
//======================================================================
// Given an exception code, returns a pointer to a static string with a 
// description of the exception                                         
//======================================================================
#define EXCEPTION( x ) case EXCEPTION_##x: return _T(#x);
LPTSTR ExceptionHandler::GetExceptionString( DWORD dwCode ){
    switch ( dwCode )    {        
		EXCEPTION( ACCESS_VIOLATION )
        EXCEPTION( DATATYPE_MISALIGNMENT )        
		EXCEPTION( BREAKPOINT )
        EXCEPTION( SINGLE_STEP )        
		EXCEPTION( ARRAY_BOUNDS_EXCEEDED )
        EXCEPTION( FLT_DENORMAL_OPERAND )        
		EXCEPTION( FLT_DIVIDE_BY_ZERO )
        EXCEPTION( FLT_INEXACT_RESULT )
        EXCEPTION( FLT_INVALID_OPERATION )        
		EXCEPTION( FLT_OVERFLOW )
        EXCEPTION( FLT_STACK_CHECK )        
		EXCEPTION( FLT_UNDERFLOW )
        EXCEPTION( INT_DIVIDE_BY_ZERO )        
		EXCEPTION( INT_OVERFLOW )
        EXCEPTION( PRIV_INSTRUCTION )        
		EXCEPTION( IN_PAGE_ERROR )
        EXCEPTION( ILLEGAL_INSTRUCTION )
        EXCEPTION( NONCONTINUABLE_EXCEPTION )        
		EXCEPTION( STACK_OVERFLOW )
        EXCEPTION( INVALID_DISPOSITION )        
		EXCEPTION( GUARD_PAGE )
        EXCEPTION( INVALID_HANDLE )    
	}
    // If not one of the "known" exceptions, try to get the string
    // from NTDLL.DLL's message table.    
	static TCHAR szBuffer[512] = { 0 };
    FormatMessage(  FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_HMODULE,
                    GetModuleHandle( _T("NTDLL.DLL") ),
                    dwCode, 0, szBuffer, sizeof( szBuffer ), 0 );
    return szBuffer;
}
//==============================================================================
// Given a linear address, locates the module, section, and offset containing  
// that address.                                                               
//                                                                             
// Note: the szModule paramater buffer is an output buffer of length specified 
// by the len parameter (in characters!)                                       
//==============================================================================
BOOL ExceptionHandler::GetLogicalAddress(
        PVOID addr, PTSTR szModule, DWORD len, DWORD& section, DWORD& offset ) {
    MEMORY_BASIC_INFORMATION mbi;
    if ( !VirtualQuery( addr, &mbi, sizeof(mbi) ) )        
		return FALSE;
    DWORD hMod = (DWORD)mbi.AllocationBase;
    if ( !GetModuleFileName( (HMODULE)hMod, szModule, len ) )
        return FALSE;    // Point to the DOS header in memory
    PIMAGE_DOS_HEADER pDosHdr = (PIMAGE_DOS_HEADER)hMod;
    // From the DOS header, find the NT (PE) header
    PIMAGE_NT_HEADERS pNtHdr = (PIMAGE_NT_HEADERS)(hMod + pDosHdr->e_lfanew);
    PIMAGE_SECTION_HEADER pSection = IMAGE_FIRST_SECTION( pNtHdr );
    DWORD rva = (DWORD)addr - hMod; // RVA is offset from module load address
    // Iterate through the section table, looking for the one that encompasses
    // the linear address.    
	for (   unsigned i = 0;
            i < pNtHdr->FileHeader.NumberOfSections;
            i++, pSection++ )    {
        DWORD sectionStart = pSection->VirtualAddress;
        DWORD sectionEnd = sectionStart
                    + max(pSection->SizeOfRawData, pSection->Misc.VirtualSize);
        // Is the address in this section???
        if ( (rva >= sectionStart) && (rva <= sectionEnd) ) {
            // Yes, address is in the section.  Calculate section and offset,
            // and store in the "section" & "offset" params, which were
            // passed by reference.            
			section = i+1;
            offset = rva - sectionStart;            
			return TRUE;        
		}    
	}
    return FALSE;   // Should never get here!
}
//============================================================
// Walks the stack, and writes the results to the report file 
//============================================================
void ExceptionHandler::IntelStackWalk( PCONTEXT pContext ) {
    _tprintf( _T("\nCall stack:\n") );
    _tprintf( _T("Address   Frame     Logical addr  Module\n") );
#ifdef _WIN64
#else
    DWORD pc = pContext->Eip;    
	PDWORD pFrame, pPrevFrame;    
    pFrame = (PDWORD)pContext->Ebp;    
	do {
        TCHAR szModule[MAX_PATH] = _T("");        
		DWORD section = 0, offset = 0;
        GetLogicalAddress((PVOID)pc, szModule,sizeof(szModule),section,offset );
        _tprintf( _T("%08X  %08X  %04X:%08X %s\n"),
                  pc, pFrame, section, offset, szModule );
        pc = pFrame[1];        
		pPrevFrame = pFrame;
        pFrame = (PDWORD)pFrame[0]; // proceed to next higher frame on stack
        if ( (DWORD)pFrame & 3 )    // Frame pointer must be aligned on a
            break;                  // DWORD boundary.  Bail if not so.
        if ( pFrame <= pPrevFrame )            
			break;
        // Can two DWORDs be read from the supposed frame address?          
        if ( IsBadWritePtr(pFrame, sizeof(PVOID)*2) )            
			break;
    } while ( 1 );
#endif
}

// Note: this function was from MSDN:
// http://msdn.microsoft.com/code/default.asp?url=/msdn-files/026/001/909/Working%20Set%20Tuner/Source%20Files/CrashHandler_cpp.asp
BOOL ExceptionHandler::InternalSymGetLineFromAddr ( IN  HANDLE			hProcess, 
													IN  DWORD			dwAddr, 
													OUT PDWORD			pdwDisplacement, 
													OUT PIMAGEHLP_LINE  Line) 
{ 
    // The problem is that the symbol engine finds only those source 
    // line addresses (after the first lookup) that fall exactly on 
    // a zero displacement. I'll walk backward 100 bytes to 
    // find the line and return the proper displacement. 
    DWORD dwTempDis = 0 ; 

    while ( FALSE == _SymGetLineFromAddr(hProcess,dwAddr - dwTempDis,pdwDisplacement,Line)) 
    { 
        dwTempDis += 1 ; 
        if ( 100 == dwTempDis ) 
        { 
            return ( FALSE ) ; 
        } 
    } 

    // I found the line, and the source line information is correct, so 
    // change the displacement if I had to search backward to find 
    // the source line. 
    if ( 0 != dwTempDis ) 
    { 
        *pdwDisplacement = dwTempDis ; 
    } 
    return ( TRUE ) ; 
} 


//============================================================
// Walks the stack, and writes the results to the report file 
//============================================================
void ExceptionHandler::ImagehlpStackWalk( PCONTEXT pContext ) {
    _tprintf( _T("\nCall stack:\n") );    
//	_tprintf( _T(_SymGetLineFromAddr ? "(Line number api available)\n" : "(Line number api not available)\n"));
	_tprintf( _T("Address   Frame\n") );
#ifdef _WIN64
#else
	SymSetOptions(SYMOPT_LOAD_LINES);

    // Could use SymSetOptions here to add the SYMOPT_DEFERRED_LOADS flag
    STACKFRAME sf;    
	memset( &sf, 0, sizeof(sf) );
    // Initialize the STACKFRAME structure for the first call.  This is only
    // necessary for Intel CPUs, and isn't mentioned in the documentation.
    sf.AddrPC.Offset       = pContext->Eip;
    sf.AddrPC.Mode         = AddrModeFlat;
    sf.AddrStack.Offset    = pContext->Esp;
    sf.AddrStack.Mode      = AddrModeFlat;
    sf.AddrFrame.Offset    = pContext->Ebp;
    sf.AddrFrame.Mode      = AddrModeFlat;    
	while ( 1 )    {
        if ( ! _StackWalk(  IMAGE_FILE_MACHINE_I386,
                            GetCurrentProcess(),
                            GetCurrentThread(),                            
							&sf,
                            pContext,                            
							0,
                            _SymFunctionTableAccess,
                            _SymGetModuleBase,                            
							0 ) )
            break;
        if ( 0 == sf.AddrFrame.Offset ) // Basic sanity check to make sure
            break;                      // the frame is OK.  Bail if not.
        _tprintf( _T("%08X  %08X  "), sf.AddrPC.Offset, sf.AddrFrame.Offset );
        // IMAGEHLP is wacky, and requires you to pass in a pointer to an
        // IMAGEHLP_SYMBOL structure.  The problem is that this structure is
        // variable length.  That is, you determine how big the structure is
        // at runtime.  This means that you can't use sizeof(struct).
        // So...make a buffer that's big enough, and make a pointer
        // to the buffer.  We also need to initialize not one, but TWO
        // members of the structure before it can be used.
        BYTE symbolBuffer[ sizeof(IMAGEHLP_SYMBOL) + 512 ];
        PIMAGEHLP_SYMBOL pSymbol = (PIMAGEHLP_SYMBOL)symbolBuffer;
        // pSymbol->SizeOfStruct = sizeof(symbolBuffer);
		pSymbol->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL);
        pSymbol->MaxNameLength = 511;
		pSymbol->Name[0] = '\0';
        DWORD symDisplacement = 0;  // Displacement of the input address,
                                    // relative to the start of the symbol

		if ( _SymGetSymFromAddr(GetCurrentProcess(), sf.AddrPC.Offset,
                               &symDisplacement, pSymbol) ) 
		{
			IMAGEHLP_LINE lineInfo;
			lineInfo.SizeOfStruct = sizeof(IMAGEHLP_LINE); // Ok m$, if you say so
			ZeroMemory ( &lineInfo, sizeof (IMAGEHLP_LINE)) ; 
			bool bGotLineNum = false;
			if (_SymGetLineFromAddr) // if this OS has the get linenum function
			{
				// call it
				bGotLineNum = InternalSymGetLineFromAddr(GetCurrentProcess(), sf.AddrPC.Offset,
										&symDisplacement, &lineInfo);
			}
			
			if (bGotLineNum)
				_tprintf( _T("%hs (%s:%d)\n"), pSymbol->Name, lineInfo.FileName, 
					lineInfo.LineNumber);
			else
				_tprintf( _T("%hs+%X\n"), pSymbol->Name, symDisplacement );
		} else {
			// No symbol found.  Print out the logical address instead.
			//DWORD thelasterr = GetLastError();
			//char nambuf[50];
			//DWORD namebuflen=49;
			//GetUserName(nambuf,&namebuflen);
			//dprintf(D_ALWAYS,"exphnd: Could not find sym %s, err=%d, user=%s\n",
			//	pSymbol->Name, thelasterr, nambuf );
            TCHAR szModule[MAX_PATH] = _T("");
            DWORD section = 0, offset = 0;
            GetLogicalAddress(  (PVOID)sf.AddrPC.Offset,
                                szModule, sizeof(szModule), section, offset );
            _tprintf( _T("%04X:%08X %s\n"),
                      section, offset, szModule );        
		} 
	}
#endif
}
//============================================================================
// Helper function that writes to the report file, and allows the user to use 
// printf style formating                                                     
//============================================================================
int __cdecl ExceptionHandler::_tprintf(const TCHAR * format, ...) {
    TCHAR szBuff[1024];    
	int retValue;    
	DWORD cbWritten;    
	va_list argptr;
              
	va_start( argptr, format );
    retValue = wvsprintf( szBuff, format, argptr );    
	va_end( argptr );
    WriteFile( m_hReportFile, szBuff, retValue * sizeof(TCHAR), &cbWritten, 0 );
    return retValue;
}
//=========================================================================
// Load IMAGEHLP.DLL and get the address of functions in it that we'll use 
//=========================================================================
BOOL ExceptionHandler::InitImagehlpFunctions( void ) {
    HMODULE hModImagehlp = LoadLibrary( _T("IMAGEHLP.DLL") );
    if ( !hModImagehlp )        
		return FALSE;
    _SymInitialize = (SYMINITIALIZEPROC)GetProcAddress( hModImagehlp,
                                                        "SymInitialize" );
    if ( !_SymInitialize )        
		return FALSE;
    _SymCleanup = (SYMCLEANUPPROC)GetProcAddress( hModImagehlp, "SymCleanup" );
    if ( !_SymCleanup )
		return FALSE;
    _StackWalk = (STACKWALKPROC)GetProcAddress( hModImagehlp, "StackWalk" );
    if ( !_StackWalk )
		return FALSE;
    _SymFunctionTableAccess = (SYMFUNCTIONTABLEACCESSPROC)
                        GetProcAddress( hModImagehlp, "SymFunctionTableAccess" );
    if ( !_SymFunctionTableAccess )
		return FALSE;
    _SymGetModuleBase=(SYMGETMODULEBASEPROC)GetProcAddress( hModImagehlp,
                                                            "SymGetModuleBase");
    if ( !_SymGetModuleBase )
		return FALSE;
    _SymGetSymFromAddr=(SYMGETSYMFROMADDRPROC)GetProcAddress( hModImagehlp,
                                                            "SymGetSymFromAddr" );
    if ( !_SymGetSymFromAddr )     
		return FALSE;

	// Note: we are not checking if _SymGetLineFromAddr is NULL because we'll just 
	// avoid calling it if it does not exist.  All the other fcns are required, 
	// this one is optional.
	_SymGetLineFromAddr= (SYMGETLINEFROMADDRPROC) GetProcAddress( hModImagehlp,
														"SymGetLineFromAddr" );

    if ( !_SymInitialize( GetCurrentProcess(), NULL, TRUE ) )
		return FALSE;

    return TRUE; 
}

