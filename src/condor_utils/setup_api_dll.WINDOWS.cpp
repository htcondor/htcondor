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
#include "setup_api_dll.WINDOWS.h"

/***************************************************************
 * Static initialization
 ***************************************************************/

SetupApiDLL::SETUPDIGETCLASSDEVS				SetupApiDLL::SetupDiGetClassDevs = NULL;
SetupApiDLL::SETUPDIENUMDEVICEINTERFACES		SetupApiDLL::SetupDiEnumDeviceInterfaces = NULL;
SetupApiDLL::SETUPDIGETDEVICEINTERFACEDETAIL	SetupApiDLL::SetupDiGetDeviceInterfaceDetail = NULL;
SetupApiDLL::SETUPDIDESTROYDEVICEINFOLIST		SetupApiDLL::SetupDiDestroyDeviceInfoList = NULL;
SetupApiDLL::SETUPDIGETDEVICEREGISTRYPROPERTY	SetupApiDLL::SetupDiGetDeviceRegistryProperty = NULL;

/***************************************************************
 * SetupApiDLL class
 ***************************************************************/

SetupApiDLL::SetupApiDLL () {
}

SetupApiDLL::~SetupApiDLL () {
	_dll.unload ();
}

bool
SetupApiDLL::load () {
	
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

	bool ok = false;

	__try {

        CHAR file_name[] = "setupapi.dll";
        if ( !_dll.load ( file_name ) ) {
            printf ( "ERROR: failed to load: %s (%d)\n", 
                file_name, GetLastError () );
            __leave;
	    }	

#if 0
		SetupDiGetClassDevs = 
			_dll.getProcAddress<SetupApiDLL::SETUPDIGETCLASSDEVS> ( 
			"SetupDiGetClassDevsA" );

		if ( !SetupDiGetClassDevs ) {
			printf ( "GetProcAddress: SetupDiGetClassDevsA\n" );
			__leave;
		}

		SetupDiEnumDeviceInterfaces = 
			_dll.getProcAddress<SetupApiDLL::SETUPDIENUMDEVICEINTERFACES> ( 
			"SetupDiEnumDeviceInterfaces" );

		if ( !SetupDiEnumDeviceInterfaces ) {
			printf ( "GetProcAddress: SetupDiEnumDeviceInterfaces\n" );
			__leave;
		}

		SetupDiGetDeviceInterfaceDetail = 
			_dll.getProcAddress<SetupApiDLL::SETUPDIGETDEVICEINTERFACEDETAIL> ( 
			"SetupDiGetDeviceInterfaceDetailA" );

		if ( !SetupDiGetDeviceInterfaceDetail ) {
			printf ( "GetProcAddress: SetupDiGetDeviceInterfaceDetailA\n" );
			__leave;
		}

		SetupDiGetDeviceRegistryProperty = 
			_dll.getProcAddress<SetupApiDLL::SETUPDIGETDEVICEREGISTRYPROPERTY> ( 
			"SetupDiGetDeviceRegistryPropertyA" );

		if ( !SetupDiGetDeviceRegistryProperty ) {
			printf ( "GetProcAddress: SetupDiGetDeviceRegistryPropertyA\n" );
			__leave;
		}

		SetupDiDestroyDeviceInfoList = 
			_dll.getProcAddress<SetupApiDLL::SETUPDIDESTROYDEVICEINFOLIST> ( 
			"SetupDiDestroyDeviceInfoList" );

		if ( !SetupDiDestroyDeviceInfoList ) {
			printf ( "GetProcAddress: SetupDiDestroyDeviceInfoList\n" );
			__leave;
		}
#endif 

        SetupDiGetClassDevs = 
            (SETUPDIGETCLASSDEVS) _dll.getProcAddress ( 
            "SetupDiGetClassDevsA" );
        
        if ( !SetupDiGetClassDevs ) {
            printf ( "GetProcAddress: SetupDiGetClassDevsA\n" );
            __leave;
        }
        
        SetupDiEnumDeviceInterfaces = 
            (SETUPDIENUMDEVICEINTERFACES) _dll.getProcAddress ( 
            "SetupDiEnumDeviceInterfaces" );
        
        if ( !SetupDiEnumDeviceInterfaces ) {
            printf ( "GetProcAddress: SetupDiEnumDeviceInterfaces\n" );
            __leave;
        }
        
        SetupDiGetDeviceInterfaceDetail = 
            (SETUPDIGETDEVICEINTERFACEDETAIL) _dll.getProcAddress ( 
            "SetupDiGetDeviceInterfaceDetailA" );
        
        if ( !SetupDiGetDeviceInterfaceDetail ) {
            printf ( "GetProcAddress: SetupDiGetDeviceInterfaceDetailA\n" );
            __leave;
        }
        
        SetupDiGetDeviceRegistryProperty = 
            (SETUPDIGETDEVICEREGISTRYPROPERTY) _dll.getProcAddress ( 
            "SetupDiGetDeviceRegistryPropertyA" );
        
        if ( !SetupDiGetDeviceRegistryProperty ) {
            printf ( "GetProcAddress: SetupDiGetDeviceRegistryPropertyA\n" );
            __leave;
        }
        
        SetupDiDestroyDeviceInfoList = 
            (SETUPDIDESTROYDEVICEINFOLIST) _dll.getProcAddress ( 
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
SetupApiDLL::unload () {
	_dll.unload ();
}

bool SetupApiDLL::loaded () const {
	return _dll.loaded ();
}

