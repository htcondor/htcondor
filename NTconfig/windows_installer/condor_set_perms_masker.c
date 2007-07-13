/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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
