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

extern "C"
int __declspec( dllexport) WINAPI KBInitialize(void)
{
	hHook=(HHOOK)SetWindowsHookEx(WH_KEYBOARD,(HOOKPROC)KBHook,g_hinstDLL,0);
	return hHook ? 1 : 0;
}

extern "C" 
int __declspec( dllexport) WINAPI KBShutdown(void)
{
	if ( UnhookWindowsHookEx(hHook) )
		return 1;	// success
	else
		return 0;	// failure
}

extern "C" 
int __declspec( dllexport)  WINAPI KBQuery(void)
{
	if ( InterlockedExchange(&KBkeyhitflag,0) )
		return 1;	// a key has been hit since last query
	else
		return 0;	// no keys hit since asked last
}

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
