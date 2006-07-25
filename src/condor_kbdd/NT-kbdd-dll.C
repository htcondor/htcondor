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

// Shared DATA
// put in here data that is needed globally
#pragma data_seg(".SHARDATA")
HHOOK hHook = NULL;
LONG KBkeyhitflag = 0;
#pragma data_seg()
#pragma comment(linker, "/SECTION:.SHARDATA,RWS")
  
__declspec(dllexport) LRESULT CALLBACK KBHook(int nCode, WPARAM wParam,
LPARAM lParam)
{
	InterlockedExchange(&KBkeyhitflag,1);

	return CallNextHookEx(hHook,nCode,wParam,lParam);
}

HINSTANCE g_hinstDLL = NULL;

#if defined(__cplusplus)
extern "C" {
#endif //__cplusplus

	
int __declspec( dllexport) WINAPI KBInitialize(void)
{
	hHook=(HHOOK)SetWindowsHookEx(WH_KEYBOARD,(HOOKPROC)KBHook,g_hinstDLL,0);
	return hHook ? 1 : 0;
}

int __declspec( dllexport) WINAPI KBShutdown(void)
{
	if ( UnhookWindowsHookEx(hHook) )
		return 1;	// success
	else
		return 0;	// failure
}

int __declspec( dllexport)  WINAPI KBQuery(void)
{
	if ( InterlockedExchange(&KBkeyhitflag,0) )
		return 1;	// a key has been hit since last query
	else
		return 0;	// no keys hit since asked last
}

#if defined(__cplusplus)
} // extern "C"
#endif //defined(__cplusplus)

BOOL WINAPI DllMain(HANDLE hInstDLL, ULONG fdwReason, LPVOID lpReserved)
{
	switch (fdwReason)
	{
		case DLL_PROCESS_ATTACH:
			g_hinstDLL = (HINSTANCE)hInstDLL;
			DisableThreadLibraryCalls(g_hinstDLL);
			break;
	}
	return 1;
}
