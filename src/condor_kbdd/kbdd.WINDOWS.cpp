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
