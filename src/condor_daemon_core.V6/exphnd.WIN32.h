/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#ifndef __EXPHND_H__
#define __EXPHND_H__

#include <imagehlp.h>

class ExceptionHandler {
	public:            
		ExceptionHandler( );      
		~ExceptionHandler( );
		void SetLogFileName( PTSTR pszLogFileName );  
		void TurnOff( );
		void TurnOn( );
		static LPTSTR GetExceptionString( DWORD dwCode );
    
	private:
      // entry point where control comes on an unhandled exception
      static LONG WINAPI MSJUnhandledExceptionFilter(
                                           PEXCEPTION_POINTERS pExceptionInfo );
      // where report info is extracted and generated    
      static void GenerateExceptionReport( PEXCEPTION_POINTERS pExceptionInfo );
      // Helper functions      
      static BOOL GetLogicalAddress(PVOID addr, PTSTR szModule, DWORD len,
                                    DWORD& section, DWORD& offset );
      static void IntelStackWalk( PCONTEXT pContext );
      static void ImagehlpStackWalk( PCONTEXT pContext );
      static int __cdecl _tprintf(const TCHAR * format, ...);  
      static BOOL InitImagehlpFunctions( void );    

	  // workaround for bug in older version of ms's dbghlp.dll 
	  static BOOL InternalSymGetLineFromAddr(IN HANDLE hProcess, IN DWORD dwAddr, 
									OUT PDWORD pdwDisplacement, 
									OUT PIMAGEHLP_LINE  Line);

      // Variables used by the class
      static TCHAR m_szLogFileName[MAX_PATH];
      static LPTOP_LEVEL_EXCEPTION_FILTER m_previousFilter;
      static HANDLE m_hReportFile;     
      // Make typedefs for some IMAGEHLP.DLL functions so that we can use them
      // with GetProcAddress
      typedef BOOL (__stdcall * SYMINITIALIZEPROC)( HANDLE, LPSTR, BOOL );
      typedef BOOL (__stdcall *SYMCLEANUPPROC)( HANDLE );
      typedef BOOL (__stdcall * STACKWALKPROC)
                   ( DWORD, HANDLE, HANDLE, LPSTACKFRAME, LPVOID,
                    PREAD_PROCESS_MEMORY_ROUTINE,PFUNCTION_TABLE_ACCESS_ROUTINE,
                    PGET_MODULE_BASE_ROUTINE, PTRANSLATE_ADDRESS_ROUTINE );
      typedef LPVOID (__stdcall *SYMFUNCTIONTABLEACCESSPROC)( HANDLE, DWORD );
      typedef DWORD (__stdcall *SYMGETMODULEBASEPROC)( HANDLE, DWORD );
      typedef BOOL (__stdcall *SYMGETSYMFROMADDRPROC)
                                    ( HANDLE, DWORD, PDWORD, PIMAGEHLP_SYMBOL );
	  typedef BOOL (__stdcall *SYMGETLINEFROMADDRPROC) 
		  (HANDLE, DWORD, PDWORD, PIMAGEHLP_LINE);


      static SYMINITIALIZEPROC _SymInitialize;
      static SYMCLEANUPPROC _SymCleanup;     
	  static STACKWALKPROC _StackWalk;
      static SYMFUNCTIONTABLEACCESSPROC _SymFunctionTableAccess;
      static SYMGETMODULEBASEPROC _SymGetModuleBase;
      static SYMGETSYMFROMADDRPROC _SymGetSymFromAddr;      
	  static SYMGETLINEFROMADDRPROC _SymGetLineFromAddr;
};

extern ExceptionHandler g_ExceptionHandler;  // global instance of class

#endif
