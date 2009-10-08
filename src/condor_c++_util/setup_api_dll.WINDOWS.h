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

#ifndef _SETUP_API_DLL_H_
#define _SETUP_API_DLL_H_

#include "load_dll.WINDOWS.h"

//
// The following uses the #pragma pack(1) directive to tell the compiler 
// to layout the members of the structure without any padding between.
// This is a requirement for the structures used by setupapi.dll functions.
//
#include <pshpack1.h>

//
// Flags controlling what is included in the device information set built
// by SetupDiGetClassDevs
//
#define DIGCF_DEFAULT           0x00000001  // only valid with DIGCF_DEVICEINTERFACE
#define DIGCF_PRESENT           0x00000002
#define DIGCF_ALLCLASSES        0x00000004
#define DIGCF_PROFILE           0x00000008
#define DIGCF_DEVICEINTERFACE   0x00000010

//
// Device registry property codes
// (Codes marked as read-only (R) may only be used for
// SetupDiGetDeviceRegistryProperty)
//
// These values should cover the same set of registry properties
// as defined by the CM_DRP codes in cfgmgr32.h.
//
// Note that SPDRP codes are zero based while CM_DRP codes are one based!
//
#define SPDRP_DEVICEDESC                  (0x00000000)  // DeviceDesc (R/W)
#define SPDRP_HARDWAREID                  (0x00000001)  // HardwareID (R/W)
#define SPDRP_COMPATIBLEIDS               (0x00000002)  // CompatibleIDs (R/W)
#define SPDRP_UNUSED0                     (0x00000003)  // unused
#define SPDRP_SERVICE                     (0x00000004)  // Service (R/W)
#define SPDRP_UNUSED1                     (0x00000005)  // unused
#define SPDRP_UNUSED2                     (0x00000006)  // unused
#define SPDRP_CLASS                       (0x00000007)  // Class (R--tied to ClassGUID)
#define SPDRP_CLASSGUID                   (0x00000008)  // ClassGUID (R/W)
#define SPDRP_DRIVER                      (0x00000009)  // Driver (R/W)
#define SPDRP_CONFIGFLAGS                 (0x0000000A)  // ConfigFlags (R/W)
#define SPDRP_MFG                         (0x0000000B)  // Mfg (R/W)
#define SPDRP_FRIENDLYNAME                (0x0000000C)  // FriendlyName (R/W)
#define SPDRP_LOCATION_INFORMATION        (0x0000000D)  // LocationInformation (R/W)
#define SPDRP_PHYSICAL_DEVICE_OBJECT_NAME (0x0000000E)  // PhysicalDeviceObjectName (R)
#define SPDRP_CAPABILITIES                (0x0000000F)  // Capabilities (R)
#define SPDRP_UI_NUMBER                   (0x00000010)  // UiNumber (R)
#define SPDRP_UPPERFILTERS                (0x00000011)  // UpperFilters (R/W)
#define SPDRP_LOWERFILTERS                (0x00000012)  // LowerFilters (R/W)
#define SPDRP_BUSTYPEGUID                 (0x00000013)  // BusTypeGUID (R)
#define SPDRP_LEGACYBUSTYPE               (0x00000014)  // LegacyBusType (R)
#define SPDRP_BUSNUMBER                   (0x00000015)  // BusNumber (R)
#define SPDRP_ENUMERATOR_NAME             (0x00000016)  // Enumerator Name (R)
#define SPDRP_SECURITY                    (0x00000017)  // Security (R/W, binary form)
#define SPDRP_SECURITY_SDS                (0x00000018)  // Security (W, SDS form)
#define SPDRP_DEVTYPE                     (0x00000019)  // Device Type (R/W)
#define SPDRP_EXCLUSIVE                   (0x0000001A)  // Device is exclusive-access (R/W)
#define SPDRP_CHARACTERISTICS             (0x0000001B)  // Device Characteristics (R/W)
#define SPDRP_ADDRESS                     (0x0000001C)  // Device Address (R)
#define SPDRP_UI_NUMBER_DESC_FORMAT       (0X0000001D)  // UiNumberDescFormat (R/W)
#define SPDRP_DEVICE_POWER_DATA           (0x0000001E)  // Device Power Data (R)
#define SPDRP_REMOVAL_POLICY              (0x0000001F)  // Removal Policy (R)
#define SPDRP_REMOVAL_POLICY_HW_DEFAULT   (0x00000020)  // Hardware Removal Policy (R)
#define SPDRP_REMOVAL_POLICY_OVERRIDE     (0x00000021)  // Removal Policy Override (RW)
#define SPDRP_INSTALL_STATE               (0x00000022)  // Device Install State (R)
#define SPDRP_LOCATION_PATHS              (0x00000023)  // Device Location Paths (R)

#define SPDRP_MAXIMUM_PROPERTY            (0x00000024)  // Upper bound on ordinals

//
// Class registry property codes
// (Codes marked as read-only (R) may only be used for
// SetupDiGetClassRegistryProperty)
//
// These values should cover the same set of registry properties
// as defined by the CM_CRP codes in cfgmgr32.h.
// they should also have a 1:1 correspondence with Device registers, where applicable
// but no overlap otherwise
//
#define SPCRP_UPPERFILTERS                (0x00000011)  // UpperFilters (R/W)
#define SPCRP_LOWERFILTERS                (0x00000012)  // LowerFilters (R/W)
#define SPCRP_SECURITY                    (0x00000017)  // Security (R/W, binary form)
#define SPCRP_SECURITY_SDS                (0x00000018)  // Security (W, SDS form)
#define SPCRP_DEVTYPE                     (0x00000019)  // Device Type (R/W)
#define SPCRP_EXCLUSIVE                   (0x0000001A)  // Device is exclusive-access (R/W)
#define SPCRP_CHARACTERISTICS             (0x0000001B)  // Device Characteristics (R/W)
#define SPCRP_MAXIMUM_PROPERTY            (0x0000001C)  // Upper bound on ordinals

//
// Device Power Information
//
#define PDCAP_D0_SUPPORTED              0x00000001
#define PDCAP_D1_SUPPORTED              0x00000002
#define PDCAP_D2_SUPPORTED              0x00000004
#define PDCAP_D3_SUPPORTED              0x00000008
#define PDCAP_WAKE_FROM_D0_SUPPORTED    0x00000010
#define PDCAP_WAKE_FROM_D1_SUPPORTED    0x00000020
#define PDCAP_WAKE_FROM_D2_SUPPORTED    0x00000040
#define PDCAP_WAKE_FROM_D3_SUPPORTED    0x00000080
#define PDCAP_WARM_EJECT_SUPPORTED      0x00000100

class SetupApiDLL {

public:

	//
	// The following signatures and structures were pulled from various
	// parts of the DDK, so that we do not depend on it for compilation.
	//
	// Note the use of __stdcall (i.e. WINAPI) in the signatures: it 
    // avoids busting the stack (i.e. the ESP register; AKA the return
    // address) since none of these functions are true 'C' (__cdecl) 
    // functions.  Stupid Windows...
	//

	//
	// Some typedefs for the setupapi.dll functions we use
	//
	typedef
	PVOID 
	(__stdcall *SETUPDIGETCLASSDEVS) ( 
		IN CONST GUID *ClassGuid,			 
		IN PCSTR Enumerator,
		IN HWND hwndParent,
		IN DWORD Flags );

	//
	// Device information structure (references a device instance
	// that is a member of a device information set)
	//
	typedef struct _SP_DEVINFO_DATA {
		DWORD cbSize;
		GUID  ClassGuid;
		DWORD DevInst;    // DEVINST handle
		ULONG_PTR Reserved;
	} SP_DEVINFO_DATA, *PSP_DEVINFO_DATA;

	//
	// Device interface information structure (references a device
	// interface that is associated with the device information
	// element that owns it).
	//
	typedef struct _SP_DEVICE_INTERFACE_DATA {
		DWORD cbSize;
		GUID  InterfaceClassGuid;
		DWORD Flags;
		ULONG_PTR Reserved;
	} SP_DEVICE_INTERFACE_DATA, *PSP_DEVICE_INTERFACE_DATA;

	typedef 
	BOOL
	(__stdcall *SETUPDIENUMDEVICEINTERFACES) (
		IN PVOID DeviceInfoSet,
		IN PSP_DEVINFO_DATA DeviceInfoData,
		IN CONST GUID *InterfaceClassGuid,
		IN DWORD MemberIndex,
		OUT PSP_DEVICE_INTERFACE_DATA DeviceInterfaceData );

	typedef struct _SP_DEVICE_INTERFACE_DETAIL_DATA {
		DWORD cbSize;
		CHAR  DevicePath[ANYSIZE_ARRAY];
	} SP_DEVICE_INTERFACE_DETAIL_DATA, *PSP_DEVICE_INTERFACE_DETAIL_DATA;

	typedef
	BOOL
	(__stdcall *SETUPDIGETDEVICEINTERFACEDETAIL) (
		IN PVOID DeviceInfoSet,
		IN PSP_DEVICE_INTERFACE_DATA DeviceInterfaceData,
		OUT PSP_DEVICE_INTERFACE_DETAIL_DATA DeviceInterfaceDetailData,
		IN DWORD DeviceInterfaceDetailDataSize,
		OUT PDWORD RequiredSize, 
		OUT PSP_DEVINFO_DATA DeviceInfoData );

	typedef
	BOOL
	(__stdcall *SETUPDIGETDEVICEREGISTRYPROPERTY)(
		IN PVOID DeviceInfoSet,
		IN PSP_DEVINFO_DATA DeviceInfoData,
		IN DWORD Property,
		OUT PDWORD PropertyRegDataType,
		OUT PBYTE PropertyBuffer,
		IN DWORD PropertyBufferSize,
		OUT PDWORD RequiredSize );

	typedef
	BOOL
	(__stdcall *SETUPDIDESTROYDEVICEINFOLIST) (
		IN PVOID DeviceInfoSet );

	SetupApiDLL ();
	virtual ~SetupApiDLL ();

	bool load ();
	bool loaded () const;
	void unload ();

	static SETUPDIGETCLASSDEVS				SetupDiGetClassDevs;
	static SETUPDIENUMDEVICEINTERFACES		SetupDiEnumDeviceInterfaces;
	static SETUPDIGETDEVICEINTERFACEDETAIL	SetupDiGetDeviceInterfaceDetail;
	static SETUPDIGETDEVICEREGISTRYPROPERTY	SetupDiGetDeviceRegistryProperty;
	static SETUPDIDESTROYDEVICEINFOLIST		SetupDiDestroyDeviceInfoList;	

private:

	LoadDLL _dll;

};

#include <poppack.h>

#endif // _SETUP_API_DLL_H_
