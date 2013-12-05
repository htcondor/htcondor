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

#ifndef __EXPHND_H__
#define __EXPHND_H__

#include <imagehlp.h>

class ExceptionHandler {
	public:            
		ExceptionHandler( );      
		~ExceptionHandler( );
		void SetLogFileName( const TCHAR * pszLogFileName );
		void SetPID( pid_t pid );  
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
	  static pid_t m_pid;
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
