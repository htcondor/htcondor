/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
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
