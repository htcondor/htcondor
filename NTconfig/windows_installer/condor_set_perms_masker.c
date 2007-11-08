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


#include <windows.h>
#include <stdio.h>
#include <errno.h>

/* get rid of some boring warnings */
#define UNUSED_VARIABLE(x) x;


int WINAPI WinMain( HINSTANCE hInstance, 
		    HINSTANCE hPrevInstance, 
		    LPSTR     lpCmdLine, 
		    int       nShowCmd ) {
  
  char                cmd_buf[1024];  
  STARTUPINFO         si;
  PROCESS_INFORMATION pi;  
  
  UNUSED_VARIABLE ( hInstance );
  UNUSED_VARIABLE ( hPrevInstance );
  UNUSED_VARIABLE ( lpCmdLine );
  UNUSED_VARIABLE ( nShowCmd ); 
  
  /* Create command line */
  sprintf ( cmd_buf, "cmd.exe /c set_perms.exe" );
  
  ZeroMemory( &si, sizeof(si) ); si.cb = sizeof(si);
  ZeroMemory( &pi, sizeof(pi) );
  
  /* Start the child process. Since we are using the cmd command, 
     we use CREATE_NO_WINDOW to hide the ugly black window from 
     the user */
  if ( !CreateProcess ( NULL, cmd_buf, NULL, NULL, FALSE,
			CREATE_NO_WINDOW, NULL, NULL, &si, &pi ) ) {    
    return EXIT_FAILURE; /* silently die ... */
  }
  
  /* Wait until child process exits. */
  WaitForSingleObject( pi.hProcess, INFINITE );
  
  /* Close process and thread handles. */
  CloseHandle( pi.hProcess );
  CloseHandle( pi.hThread );
  
  return EXIT_SUCCESS;

}
