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
#include "ipv6_hostname.h"

#include <iphlpapi.h>

/***************************************************************
 * Ripped from the MS DDK
 ***************************************************************/

/*  {6B29FC40-CA47-1067-B31D-00DD010662DA}
    Length = 1[{] + 32[sizeof(GUID)*2] + 4[-] + 1[}]
    */
#define GUID_STR_LENGTH ((sizeof(GUID)*2)+6)

#define CONDOR_DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
    extern const GUID name \
                = { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }

/*    Guid for Lan Class. (From DDK)
*/
CONDOR_DEFINE_GUID(CONDOR_GUID_NDIS_LAN_CLASS, 0xad498944, 0x762f,
                    0x11d0, 0x8d, 0xcb, 0x00, 0xc0, 0x4f, 0xc3,
                    0x35, 0x8c);

/***************************************************************
 * Defines
 ***************************************************************/

#define WAKE_FROM_ANY_SUPPORTED (PDCAP_WAKE_FROM_D0_SUPPORTED| \
                                 PDCAP_WAKE_FROM_D1_SUPPORTED| \
                                 PDCAP_WAKE_FROM_D2_SUPPORTED| \
                                 PDCAP_WAKE_FROM_D3_SUPPORTED)

/***************************************************************
 * WindowsNetworkAdapter class
 ***************************************************************/

WindowsNetworkAdapter::WindowsNetworkAdapter (void) noexcept
: _exists ( false ) {
    // TODO: Picking IPv4 arbitrarily.
	std::string my_ip = get_local_ipaddr(CP_IPV4).to_ip_string();
    strncpy ( _ip_address, my_ip.c_str(), IP_STRING_BUF_SIZE );
    _description[0] = '\0';
}

WindowsNetworkAdapter::WindowsNetworkAdapter ( const condor_sockaddr & ip_addr)
	noexcept
: _exists ( false ) {
    strncpy ( _ip_address, ip_addr.to_ip_string().c_str(), IP_STRING_BUF_SIZE );
    _description[0] = '\0';
}

WindowsNetworkAdapter::WindowsNetworkAdapter ( LPCSTR description ) noexcept
: _exists ( false ) {
    strncpy ( _description, description,
        MAX_ADAPTER_DESCRIPTION_LENGTH + 4 );
    _ip_address[0] = '\0';
}

WindowsNetworkAdapter::~WindowsNetworkAdapter (void) noexcept {
}

/***************************************************************
 * WindowsNetworkAdapter class
 ***************************************************************/

bool
WindowsNetworkAdapter::initialize (void) {

    PIP_ADAPTER_INFO    adapters    = NULL,
                        current     = NULL;
	DWORD               error       = 0,
                        supports    = 0;
    UINT                length      = 0;
	ULONG               size;
    LPCSTR              other       = NULL;
    LPSTR               offset      = _hardware_address;
    unsigned int        i           = 0;
    PCM_POWER_DATA      power_data  = NULL;
    bool                ok          = false;

    __try {

        /* allocate a very small ammount of ram, which will cause the
           call for information to fail, but will reveal the size
           of the true structure */
        size = sizeof ( IP_ADAPTER_INFO );
        adapters = (PIP_ADAPTER_INFO) malloc ( size );

        if ( ERROR_SUCCESS != GetAdaptersInfo ( adapters, &size ) ) {
            free ( adapters );
            adapters = (PIP_ADAPTER_INFO) malloc ( size );
	    }

        /* now attempt to grab all the information we can for each
           adapter, then search through them until we find the one
           which matches the IP we are looking for */
        if ( NO_ERROR != ( error = GetAdaptersInfo ( adapters, &size ) ) ) {

            /* failed to get the adapter information */
            __leave;

        } else {

            /* initialize the Setup DLL */
            if ( !_setup_dll.load () ) {

                dprintf (
                    D_FULLDEBUG,
                    "WindowsNetworkAdapter::initialize: "
                    "Failed to load setup.dll which is required to "
                    "gather power information for network "
                    "adapters.\n" );

                __leave;

            }

            /* reset any bits set from a previous initialization */
            wolResetSupportBits ();
            wolResetEnableBits ();

            /* got it, lets run through them and the one we
            are interested in */
            current = adapters;

            while ( current ) {

                char *ip = current->IpAddressList.IpAddress.String,
                     *description = current->Description;

                if ( MATCH == strcmp ( _ip_address, ip ) ||
                     MATCH == strcmp ( _description, description ) ) {

                    /* we do exist! */
                    _exists = true;

                    /* record the adapter GUID */
                    strncpy (
                        _adapter_name,
                        current->AdapterName,
                        MAX_ADAPTER_NAME_LENGTH + 4 );

                    /* using the GUID, get the device's power
                    capabilities */
                    power_data = getPowerData ();
                    if ( power_data ) {

                        /* shorten the variable we need to use */
                        supports = power_data->PD_Capabilities;

                        /* do we support waking from any state? */
                        if ( supports & WAKE_FROM_ANY_SUPPORTED ) {

                            dprintf (
                                D_FULLDEBUG,
                                "WOL_MAGIC enabled\n" );

                            wolEnableSupportBit ( WOL_MAGIC );
                            wolEnableEnableBit ( WOL_MAGIC );
                        }

                        LocalFree ( power_data );
                        power_data = NULL;

                    }

                    /* copy over the adapter's subnet */
                    strncpy (
                        _subnet_mask,
                        current->IpAddressList.IpMask.String,
                        IP_STRING_BUF_SIZE );

                    /* finally, format the hardware address in the
                    format our tools expect */
                    length = current->AddressLength;
                    for ( i = 0; i < length; ++i ) {
                        sprintf ( offset, "%.2X%c",
                            current->Address[i],
				            i == length - 1 ? '\0' : ':' );
                        offset += 3;
                    }

                    dprintf (
                        D_FULLDEBUG,
                        "Hardware address: '%s'\n",
                        _hardware_address );

                    /* if we got here, then the world is a happy place */
                    ok = true;

                    /* we found the one we want, so bail out early */
                    __leave;

                }

                /* move on to the next one */
                current = current->Next;

            }

        }

    }
    __finally {

        if ( adapters ) {
            free ( adapters );
        }

    }

    return ok;

}

const char*
WindowsNetworkAdapter::hardwareAddress (void) const {
    return _hardware_address;
}

condor_sockaddr
WindowsNetworkAdapter::ipAddress (void) const {
    return condor_sockaddr::null; /* not used on Windows */
}

const char*
WindowsNetworkAdapter::interfaceName () const
{
	return _description; // _adapter_name;
}

const char*
WindowsNetworkAdapter::subnetMask (void) const {
    return _subnet_mask;
}

bool
WindowsNetworkAdapter::exists (void) const {
    return _exists;
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
WindowsNetworkAdapter::getPowerData (void) const {

    return (PCM_POWER_DATA) getRegistryProperty (
        SPDRP_DEVICE_POWER_DATA, &processPowerData );

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
                &CONDOR_GUID_NDIS_LAN_CLASS,
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
                    &CONDOR_GUID_NDIS_LAN_CLASS,
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
                ASSERT( pdidd != NULL ); // this is a no-op, but makes warnings go away

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
                        _adapter_name,
                        short_name,
                        GUID_STR_LENGTH ) ) {

                    //
                    // If this is not the guid we are looking for,
                    // then keep on searching...
                    //
                    LocalFree ( pdidd );
                    continue;

                }

                //dprintf ( D_FULLDEBUG, "DeviceName: %s\n", _adapter_name );

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
