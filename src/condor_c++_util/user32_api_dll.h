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

#pragma once

#ifndef _USER32_API_H_
#define _USER32_API_H_

#include "load_dll.WINDOWS.h"

class User32API {
    
    //
	// The following signatures and structures were pulled from various
	// parts of the SDK, so that we do not depend on it for compilation.
	//
	// Note the use of __stdcall in the signatures: it avoids busting the 
	// stack (i.e. the ESP register; AKA the return address) since none of 
	// these functions are 'C' (__cdecl) functions.  Stupid Windows...
	//

	//
	// Some typedefs for the user32.dll functions we use
	//

	typedef  PVOID           HPOWERNOTIFY;
	typedef  HPOWERNOTIFY   *PHPOWERNOTIFY;

	typedef
	HPOWERNOTIFY 
	(__stdcall *REGISTERPOWERSETTINGNOTIFICATION) ( 
		IN  HANDLE hRecipient,
		IN  LPCGUID PowerSettingGuid,
		IN  DWORD Flags );

	typedef
	BOOL
	(__stdcall *UNREGISTERPOWERSETTINGNOTIFICATION) (
		IN HPOWERNOTIFY Handle
    );

	User32API () throw ();
	virtual ~User32API () throw ();

    bool load ();
    bool loaded () const;
	void unload ();

    static REGISTERPOWERSETTINGNOTIFICATION	  RegisterPowerSettingNotification;
	static UNREGISTERPOWERSETTINGNOTIFICATION UnregisterPowerSettingNotification;
    
private:

    LoadDLL _dll;

};

#endif // _USER32_API_H_
