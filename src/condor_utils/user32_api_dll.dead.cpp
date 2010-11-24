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

/***************************************************************
 * Headers
 ***************************************************************/

#include "condor_common.h"
#include "user32_api_dll.h"

/***************************************************************
 * Static initialization
 ***************************************************************/

User32API::REGISTERPOWERSETTINGNOTIFICATION		User32API::RegisterPowerSettingNotification = NULL;
User32API::UNREGISTERPOWERSETTINGNOTIFICATION	User32API::UnregisterPowerSettingNotification = NULL;

/***************************************************************
 * User32API class
 ***************************************************************/

User32API::User32API () {
}


User32API::~User32API () {
    _dll.unload ();
}

bool
User32API::load () {

    CHAR            file_name[] = "user32.dll";
	OSVERSIONINFO   os_version = { sizeof ( OSVERSIONINFO ) };
    bool            ok = false;
    
    __try {

        //
        // Figure out if we are running on Vista or above: only those platforms
        // support the query types we rely upon.
        //        
        if ( GetVersionEx ( &os_version ) ) {
            
            if ( os_version.dwMajorVersion < 6 ) {
                
                dprintf ( 
                    D_ALWAYS.
                    "User32API::load: Must be using Vista or above.\n" );
                
                __leave;
            }

	    }
        
        
        if ( !_dll.load ( file_name ) ) {
            printf ( "ERROR: failed to load: %s (%d)\n", 
                file_name, GetLastError () );
            __leave;
	    }
        
        RegisterPowerSettingNotification = 
            _dll.getProcAddress<User32API::REGISTERPOWERSETTINGNOTIFICATION> ( "RegisterPowerSettingNotification" );
        
        if ( !UnregisterPowerSettingNotification ) {
            printf ( "GetProcAddress: RegisterPowerSettingNotification\n" );
            __leave;
        }
        
        UnregisterPowerSettingNotification = 
            _dll.getProcAddress<User32API::UNREGISTERPOWERSETTINGNOTIFICATION> ( "UnregisterPowerSettingNotification" );
        
        if ( !UnregisterPowerSettingNotification ) {
            printf ( "GetProcAddress: UnregisterPowerSettingNotification\n" );
            __leave;
		}
        
    }
    
    
	__finally {

		if ( !ok ) {
			unload ();
		}

	}

}


bool
User32API::load () {
	
	/*	Figure out if we are running on XP or above: only those 
		platforms support the query types we rely upon. 
	*/
	/*
    OSVERSIONINFO info = { sizeof ( OSVERSIONINFO ) };
	if ( GetVersionEx ( &info ) ) {
		if ( info.dwMajorVersion < 5 
			|| ( 5 == info.dwMajorVersion && info.dwMinorVersion < 1 ) ) {
				printf ( "ERROR: Must be using XP or above.\n" );
				return false;
		}
	}

    //
    // [9/9/2008] Seems an updated Win2K system (MSI 3.1) will expose
    //            the proper API, even if MSDN says it is not 
    //            supported. -Ben
    //
    */

	CHAR file_name[] = "setupapi.dll";
	if ( !_dll.load ( file_name ) ) {
		printf ( "ERROR: failed to load: %s (%d)\n", 
			file_name, GetLastError () );
		return false;
	}	

	bool ok = false;

	__try {

        

        

		SetupDiGetClassDevs = 
			_dll.getProcAddress<User32API::SETUPDIGETCLASSDEVS> ( 
			"SetupDiGetClassDevsA" );

		if ( !SetupDiGetClassDevs ) {
			printf ( "GetProcAddress: SetupDiGetClassDevsA\n" );
			__leave;
		}

		SetupDiEnumDeviceInterfaces = 
			_dll.getProcAddress<User32API::SETUPDIENUMDEVICEINTERFACES> ( 
			"SetupDiEnumDeviceInterfaces" );

		if ( !SetupDiEnumDeviceInterfaces ) {
			printf ( "GetProcAddress: SetupDiEnumDeviceInterfaces\n" );
			__leave;
		}

		SetupDiGetDeviceInterfaceDetail = 
			_dll.getProcAddress<User32API::SETUPDIGETDEVICEINTERFACEDETAIL> ( 
			"SetupDiGetDeviceInterfaceDetailA" );

		if ( !SetupDiGetDeviceInterfaceDetail ) {
			printf ( "GetProcAddress: SetupDiGetDeviceInterfaceDetailA\n" );
			__leave;
		}

		SetupDiGetDeviceRegistryProperty = 
			_dll.getProcAddress<User32API::SETUPDIGETDEVICEREGISTRYPROPERTY> ( 
			"SetupDiGetDeviceRegistryPropertyA" );

		if ( !SetupDiGetDeviceRegistryProperty ) {
			printf ( "GetProcAddress: SetupDiGetDeviceRegistryPropertyA\n" );
			__leave;
		}

		SetupDiDestroyDeviceInfoList = 
			_dll.getProcAddress<User32API::SETUPDIDESTROYDEVICEINFOLIST> ( 
			"SetupDiDestroyDeviceInfoList" );

		if ( !SetupDiDestroyDeviceInfoList ) {
			printf ( "GetProcAddress: SetupDiDestroyDeviceInfoList\n" );
			__leave;
		}

        /* if we got here, then we have all the calls we need */
        ok = true;

	}
	__finally {

		if ( !ok ) {
			unload ();
		} 

	}

	return ok;

}

void 
User32API::unload () {
	_dll.unload ();
}

bool User32API::loaded () const {
	return _dll.loaded ();
}
