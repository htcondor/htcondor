/***************************************************************
*
* Copyright (C) 1990-2008, Condor Team, Computer Sciences Department,
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

#include "condor_common.h"
#include "network_adapter.WINDOWS.h"

/***************************************************************
* Ripped from the MS DDK
***************************************************************/

/*    {6B29FC40-CA47-1067-B31D-00DD010662DA}
    Length = 1[{] + 32[sizeof(GUID)*2] + 4[-] + 1[}]
    */
#define GUID_STR_LENGTH ((sizeof(GUID)*2)+6)

/*    Guid for Lan Class. (From DDK)
*/
DEFINE_GUID(GUID_NDIS_LAN_CLASS, 0xad498944, 0x762f, 0x11d0, 0x8d, 
            0xcb, 0x00, 0xc0, 0x4f, 0xc3, 0x35, 0x8c);

/***************************************************************
* WindowsNetworkAdapter class
***************************************************************/

WindowsNetworkAdapter::WindowsNetworkAdapter ( LPSTR device_name ) throw () {
    strncpy ( _device_name, device_name, _device_name_length );
}

WindowsNetworkAdapter::~WindowsNetworkAdapter () throw () {
}

/*    The power data registry values requires some preprocessing before 
    it can be queried, so we allow a user to specify a function to handle 
    preprocessing.
*/
static void 
processPowerData ( IN PBYTE p ) {
    PCM_POWER_DATA ppd = (PCM_POWER_DATA) p;
    ppd->PD_Size = sizeof ( ppd );
}

PCM_POWER_DATA 
WindowsNetworkAdapter::getPowerData () const {

    return (PCM_POWER_DATA) getRegistryProperty ( 
        SPDRP_DEVICE_POWER_DATA, &processPowerData );

}

PCHAR 
WindowsNetworkAdapter::getDriverKey () const {
    
    return (PCHAR) getRegistryProperty ( SPDRP_DRIVER );

}

PBYTE 
WindowsNetworkAdapter::getRegistryProperty ( 
    IN DWORD registry_property,
    IN PRE_PROCESS_REISTRY_VALUE preprocess ) const {

        PVOID   device_information      = INVALID_HANDLE_VALUE;
        DWORD   error                   = NO_ERROR, 
                required                = 0,
                index                   = 0,
                last_error              = ERROR_SUCCESS;
        SetupApiDLL::PSP_DEVICE_INTERFACE_DETAIL_DATA pdidd = NULL;
        PCHAR   short_name              = NULL;
        PBYTE   value                   = NULL;
        BOOL    enumerated_devices      = FALSE,
                got_device_details      = FALSE,
                got_registry_property   = FALSE,    
                ok                      = FALSE;

        __try {

            device_information = SetupApiDLL::SetupDiGetClassDevs ( 
                &GUID_NDIS_LAN_CLASS, 
                0, 
                0, 
                DIGCF_PRESENT | DIGCF_DEVICEINTERFACE );

            if ( INVALID_HANDLE_VALUE == device_information ) {
                
                last_error = GetLastError ();

                dprintf ( 
                    D_FULLDEBUG, 
                    "WindowsNetworkAdapter::getRegistryProperty: "
                    "SetupDiGetClassDevs call failed: "
                    "(last-error = %d)\n",
                    last_error );

                __leave;

            }

            while ( true ) {

                //
                // We loop indefinitely because the number of 
                // interfaces is only known to us when we run 
                // out of them: it is at this time that we 
                // break out of the loop.
                //

                SetupApiDLL::SP_DEVICE_INTERFACE_DATA did = { 
                    sizeof ( SetupApiDLL::SP_DEVICE_INTERFACE_DATA ) };

                enumerated_devices = SetupApiDLL::SetupDiEnumDeviceInterfaces (
                    device_information,
                    NULL,
                    &GUID_NDIS_LAN_CLASS,
                    index++,
                    &did );            

                if ( !enumerated_devices ) {

                    last_error = GetLastError ();

                    //
                    // Perhaps there are no more interfaces, in 
                    // which case it is ok that we failed.
                    //
                    if ( ERROR_NO_MORE_ITEMS == last_error ) {                
                        
                        ok = TRUE;

                    }

                    __leave;

                } 

                SetupApiDLL::SP_DEVINFO_DATA dd 
                    = { sizeof ( SetupApiDLL::SP_DEVINFO_DATA ) };

                got_device_details = SetupApiDLL::SetupDiGetDeviceInterfaceDetail ( 
                    device_information,
                    &did,
                    NULL,
                    0,
                    &required,
                    &dd );    // MSDN says this is optional,
                              // but it is not... here...

                if ( !got_device_details ) {

                    last_error = GetLastError ();

                    if ( ERROR_INSUFFICIENT_BUFFER == last_error ) {

                        pdidd = (SetupApiDLL::PSP_DEVICE_INTERFACE_DETAIL_DATA)
                            LocalAlloc ( 
                                LPTR, 
                                required );

                        if  ( !pdidd ) {
                            
                            last_error = GetLastError ();
                            
                            dprintf ( 
                                D_FULLDEBUG, 
                                "WindowsNetworkAdapter::"
                                "getRegistryProperty: "
                                "LocalAlloc call failed: "
                                "(last-error = %d)\n",
                                last_error );
                            
                            __leave;

                        }

                        pdidd->cbSize = sizeof ( *pdidd );

                    } else {

                        dprintf ( 
                            D_FULLDEBUG, 
                            "WindowsNetworkAdapter::"
                            "getRegistryProperty: "
                            "SetupDiGetDeviceInterfaceDetail (1st) "
                            "call failed: (last-error = %d)\n",
                            last_error );
                        
                        __leave;

                    }

                } 

                got_registry_property = SetupApiDLL::SetupDiGetDeviceInterfaceDetail (
                    device_information,
                    &did,
                    pdidd,
                    required,
                    NULL,
                    NULL ); // here, however, it *is* optional?!
                            // (WTF!?!--pardon my French)

                if ( !got_registry_property ) {
                    
                    last_error = GetLastError ();
                    
                    dprintf ( 
                        D_FULLDEBUG, 
                        "WindowsNetworkAdapter::getRegistryProperty: "
                        "SetupDiGetDeviceInterfaceDetail (2nd) call "
                        "failed: (last-error = %d)\n",
                        last_error );

                    __leave;

                }

                //dprintf ( D_FULLDEBUG, "DevicePath: %s\n", pdidd->DevicePath );

                // 
                // Device paths are of the form:
                //
                // \\?\root#ms_ptiminiport#0000#{ad498944-762f-11d0-8dcb \
                //   -00c04fc3358c}\{5a8445c2-ecd0-407d-a359-e1a98fb299b4}
                //
                // Where the final guid is the device name or 
                // "id"; that is to say: it is the part we are 
                // interested in.
                //
                short_name = strrchr ( 
                    pdidd->DevicePath, 
                    '\\' ) + 1;

                if ( 0 != strnicmp ( 
                        _device_name, 
                        short_name, 
                        GUID_STR_LENGTH ) ) {

                    //
                    // If this is not the guid we are looking for,
                    // then keep on searching...
                    //
                    LocalFree ( pdidd );
                    continue;

                }

                //dprintf ( D_FULLDEBUG, "DevicePath: %s\n", pdidd->DevicePath );

                got_registry_property = SetupApiDLL::SetupDiGetDeviceRegistryProperty (
                    device_information,
                    &dd,
                    registry_property,
                    NULL,
                    NULL,
                    0,
                    &required );

                //dprintf ( D_FULLDEBUG, "registry_property required: %d\n", required );

                if ( !got_registry_property ) {

                    last_error = GetLastError ();

                    if ( ERROR_INSUFFICIENT_BUFFER == last_error ) {

                        value = (PBYTE) LocalAlloc ( 
                            LPTR, 
                            required );

                        if  ( !value ) {

                            last_error = GetLastError ();
                            
                            dprintf ( 
                                D_FULLDEBUG, 
                                "WindowsNetworkAdapter::"
                                "getRegistryProperty: "
                                "LocalAlloc call "
                                "failed: (last-error = %d)\n",
                                last_error );
                            
                            __leave;

                        }

                    } else {

                        last_error = GetLastError ();
                        
                        dprintf ( 
                            D_FULLDEBUG, 
                            "WindowsNetworkAdapter::"
                            "getRegistryProperty: "
                            "SetupDiGetDeviceRegistryProperty (1st) "
                            "call failed: (last-error = %d)\n",
                            last_error );
                        
                        __leave;

                    }

                } 

                //
                // Do any preprocessing that may be required
                //
                if ( preprocess ) {
                    preprocess ( value );
                }

                got_registry_property = SetupApiDLL::SetupDiGetDeviceRegistryProperty (
                    device_information,
                    &dd,
                    registry_property,
                    NULL,
                    value,
                    required,
                    NULL );

                if ( !got_registry_property ) {

                    last_error = GetLastError ();

                    dprintf ( 
                        D_FULLDEBUG, 
                        "WindowsNetworkAdapter::getRegistryProperty: "
                        "SetupDiGetDeviceRegistryProperty (2nd) "
                        "call failed: (last-error = %d)\n",
                        last_error );

                    __leave;

                }            

                // 
                // If we get here, then it means we have found the 
                // device information we were looking for and can 
                // return it.
                //
                ok = TRUE;

                __leave;

            }             

        }
        __finally {

            //
            // Propagate the last error 
            //
            SetLastError ( ok ? ERROR_SUCCESS : last_error );

            if ( INVALID_HANDLE_VALUE != device_information ) {
                SetupApiDLL::SetupDiDestroyDeviceInfoList ( device_information );
            }

            if ( pdidd ) {
                LocalFree ( pdidd );
                pdidd = NULL;
            }

            if ( !ok && value ) {
                LocalFree ( value );
                value = NULL;
            }

        }

        //
        // If we found the device information, the following will be a 
        // valid pointer to it; otherwise, it will be NULL.
        //
        return value;

}
