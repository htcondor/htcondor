/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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



#include "condor_common.h"
#ifdef WIN32

#include "windows_firewall.h"
#include "basename.h"


WindowsFirewallHelper::WindowsFirewallHelper() {
	fwProf = NULL;
	fwApps = NULL;
    fwMgr  = NULL;
	fwPolicy = NULL;
	wfh_initialized = false;
	wfh_operational = false;
}

WindowsFirewallHelper::~WindowsFirewallHelper() {
	
	WindowsFirewallCleanup();
	
}

bool
WindowsFirewallHelper::ready() {

	/* initialized == have we run init() yet? */
	/* operational == does the firewall stuff work on this OS? */

	if ( ! wfh_initialized ) {
		wfh_operational = init();
	}

	return wfh_operational;
}

bool
WindowsFirewallHelper::init() {
	
	HRESULT hr;
	bool result;

	result = true;

	if ( wfh_initialized ) {

		// already initialized
		return true;
	}
	
	 // Initialize COM.
    hr = CoInitializeEx(NULL,
            COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE
            );
    if (SUCCEEDED(hr) || RPC_E_CHANGED_MODE == hr) {
        hr = S_OK;
    } else {
        dprintf(D_ALWAYS, "WinFirewall: CoInitializeEx failed: 0x%08lx\n", hr);
        result = false;
    }
	
	if ( ! WindowsFirewallInitialize() ) {
		dprintf(D_FULLDEBUG, "Warning: Error initializing firewall profile.\n");
		result = false;
	}

	wfh_initialized = true;

	return result;
}

bool 
WindowsFirewallHelper::firewallIsOn() {

	HRESULT hr;
    VARIANT_BOOL fwEnabled;

	hr = S_OK;

	if ( ! ready() ) { 
		return false;
	}

    // Get the current state of the firewall.
    hr = fwProf->get_FirewallEnabled(&fwEnabled);
    if (FAILED(hr))
    {
        dprintf(D_ALWAYS, "WinFirewall: get_FirewallEnabled failed: 0x%08lx\n", hr);
        return false;
    }

    return (fwEnabled == VARIANT_TRUE);    
}

bool 
WindowsFirewallHelper::addTrusted( const char *app_path ) {

	const char *app_basename;
	BSTR app_path_bstr = NULL;
	BSTR app_basename_bstr = NULL;

	HRESULT hr = S_OK;
	INetFwAuthorizedApplication* fwApp = NULL;
	
	if ( ! ready() ) { 
		return false;
	}

	if ( !firewallIsOn() ) {
		// firewall is turned off, so there's nothing to do.
		return true;
	}

	if ( applicationIsTrusted(app_path) ) {
		// this app is already set to be trusted, so do nothing.
		return true;
	}

	// now, if the basename of the app is condor_<something>, we 
	// want to make sure there aren't any other entries of the same
	// condor daemon with a different path. We only do this for "condor_" 
	// executables as a safety to keep us from removing trusted applications
	// that have nothing to do with condor.
	app_basename = condor_basename(app_path);

	if ( _strnicmp(app_basename, "condor_", strlen("condor_")) == 0 ) {
		
		removeByBasename(app_basename);

	}

	// now just add the application to the trusted list.
	
    // Create an instance of an authorized application.
    hr = CoCreateInstance(
		__uuidof(NetFwAuthorizedApplication),
		NULL,
		CLSCTX_INPROC_SERVER,
		__uuidof(INetFwAuthorizedApplication),
		reinterpret_cast<void**>
		(static_cast<INetFwAuthorizedApplication**>(&fwApp))
		);
	if (FAILED(hr))
	{
		dprintf(D_FULLDEBUG, "WinFirewall: CoCreateInstance failed: 0x%08lx\n", hr);
		return false;
	}
	
	app_path_bstr = charToBstr(app_path);
	// Set the process image file name.
	hr = fwApp->put_ProcessImageFileName(app_path_bstr);
	if (FAILED(hr))
	{
		if ( hr == 0x80070002 ) {
			dprintf(D_ALWAYS, "WinFirewall Error: Could not find trusted app image %s\n",
				app_path);
		} else {
			dprintf(D_ALWAYS, "put_ProcessImageFileName failed: 0x%08lx\n", hr);
		}
		goto error;
	}
	
        // Allocate a BSTR for the application friendly name.
        app_basename_bstr = charToBstr(app_basename);

        // Set the application friendly name.
        hr = fwApp->put_Name(app_basename_bstr);
        if (FAILED(hr))
        {
            dprintf(D_ALWAYS, "WinFirewall: put_Name failed: 0x%08lx\n", hr);
            goto error;
        }

        // Add the application to the collection.
        hr = fwApps->Add(fwApp);
        if (FAILED(hr))
        {
           dprintf(D_ALWAYS, "WinFirewall: Add failed: 0x%08lx\n", hr);
            goto error;
        }

		// it seems like we should always inform users somehow that we're 
		// doing this.
        dprintf(D_ALWAYS, "Authorized application %s is now enabled in the"
			   " firewall.\n",
            app_path );

error:

    // Free the BSTRs.
    SysFreeString(app_path_bstr);
    SysFreeString(app_basename_bstr);

    // Release the authorized application instance.
    if (fwApp != NULL)
    {
        fwApp->Release();
    }

    return (SUCCEEDED(hr));

}

bool
WindowsFirewallHelper::applicationIsTrusted(const char* app_path) {

	HRESULT hr;
    BSTR app_path_bstr = NULL;
    VARIANT_BOOL fwEnabled;
    INetFwAuthorizedApplication* fwApp = NULL;
	bool result;

	result = false;

	if ( ! ready() ) {
		return false;
	}

	app_path_bstr = charToBstr(app_path);

    // Attempt to retrieve the authorized application.
    hr = fwApps->Item(app_path_bstr, &fwApp);
    if (SUCCEEDED(hr))
    {
        // Find out if the authorized application is enabled.
        hr = fwApp->get_Enabled(&fwEnabled);
        if (FAILED(hr))
        {
            dprintf(D_ALWAYS, "WinFirewall: get_Enabled failed: 0x%08lx\n", hr);
			result = false;
        } else {
			result = (fwEnabled == VARIANT_TRUE);
		}
	}
        
    // Free the BSTR.
    SysFreeString(app_path_bstr);

    // Release the authorized application instance.
    if (fwApp != NULL)
    {
        fwApp->Release();
    }

	return result;
}

bool 
WindowsFirewallHelper::removeByBasename( const char *name ) {
	
	HRESULT hr = S_OK;
	 
	IUnknown* pUnknown = NULL;
	IEnumVARIANT* pEnum = NULL;
    INetFwAuthorizedApplication* fwApp = NULL;
 
	bool result = true;
	long count;
	unsigned long cnt;
	int i;
	VARIANT v;

	if ( ! ready() ) {
		return false;
	}

	hr = fwApps->get__NewEnum(&pUnknown);
	if ( hr != S_OK ) {
		dprintf(D_ALWAYS, "Failed to get enumerator for Authorized "
				"Applications (err=%x)\n", hr);
		return false;
	}

	hr = fwApps->get_Count(&count);
	if ( hr != S_OK ) {
		dprintf(D_ALWAYS, "Failed to get count of Authorized "
				"Applications (err=%x)\n", hr);
		return false;
	}

	hr = pUnknown->QueryInterface(IID_IEnumVARIANT, (void**)&pEnum);
	if ( hr != S_OK ) {
		if ( hr == E_NOINTERFACE ) {
			dprintf(D_ALWAYS, "Failed to QueryInterface for trusted "
					"applications. Interface not supported.\n");
		} else {
			dprintf(D_ALWAYS, "Failed to QueryInterface for trusted "
				   "applications. (err=%x)\n", hr);
		}
		return false;
	}

	for (i=0; i<count; i++) {
		BSTR str = NULL;
		int len;
		char *tmp;
	    const char *bn;

		hr = pEnum->Next(1, &v, &cnt);
		// interesting tidbit: Microsoft says Enum->Next() function
		// either returns S_OK or S_FALSE. Funny, on Win2k3 SP1
		// it returns 0x80020008, or Bad Variable Type. Sigh.
		if ( hr != S_OK ) {
			// no more elements. stop.
			break;
		}

		fwApp = (INetFwAuthorizedApplication*)v.byref;

		fwApp->get_ProcessImageFileName(&str);
		printf("Result is %lS\n", str);

		len = wcslen(str);
		tmp = (char*) malloc(len+1 * sizeof(char));

		sprintf(tmp, "%S", str);

		bn = condor_basename(tmp);

		if ( 0 == stricmp(bn, name) ) {
			
			// basenames match, so remove it from the list.
			
			result = removeTrusted(tmp);
		}
		free(tmp);

		SysFreeString(str);
	}

	return result;
}

bool 
WindowsFirewallHelper::removeTrusted( const char *app_path ) {
	
	BSTR app_path_bstr = NULL;
	HRESULT hr = S_OK;
	bool result = false;

	if ( ! ready() ) {
		return false;
	}

	app_path_bstr = charToBstr(app_path);

	if ( app_path_bstr == NULL ) {
		return false;
	}

    // Attempt to retrieve the authorized application.
    hr = fwApps->Remove(app_path_bstr);
    if (SUCCEEDED(hr)) {

        result = true;	

	} else if (FAILED(hr)) {

        dprintf(D_ALWAYS, "WinFirewall: remove trusted app failed: 0x%08lx\n",
			hr);
		result = false;
	}
        
    // Free the BSTR.
    SysFreeString(app_path_bstr);

  	return result;
}

BSTR
WindowsFirewallHelper::charToBstr(const char* str) {

	LPWSTR wstr;
	BSTR the_bstr;
	HRESULT hr = S_OK;

	    // Allocate a BSTR for the process image file name.
	wstr = (LPWSTR)malloc((strlen(str)+1)*sizeof(WCHAR));
	swprintf(wstr, L"%S", str);

    the_bstr = SysAllocString(wstr);
	
	// get rid of this dumb string right away so we don't leak it.
	free(wstr);
	wstr = NULL;
    
	if (SysStringLen(the_bstr) == 0) {
        hr = E_OUTOFMEMORY;
        dprintf(D_ALWAYS, "WinFirewall: SysAllocString failed: 0x%08lx\n", hr);
        return NULL;
    }

	return the_bstr;
}

bool 
WindowsFirewallHelper::WindowsFirewallInitialize() {

	HRESULT hr;

	hr = S_OK;

    // Create an instance of the firewall settings manager.
    hr = CoCreateInstance(
            __uuidof(NetFwMgr),
            NULL,
            CLSCTX_INPROC_SERVER,
            __uuidof(INetFwMgr),
            reinterpret_cast<void**>(static_cast<INetFwMgr**>(&fwMgr))
            );
    if (FAILED(hr))
    {
        dprintf(D_FULLDEBUG, 
				"WinFirewall: CoCreateInstance failed: 0x%08lx\n", hr);
		return false;
    }

    // Retrieve the local firewall policy.
    hr = fwMgr->get_LocalPolicy(&fwPolicy);
    if (FAILED(hr))
    {
        dprintf(D_ALWAYS, "WinFirewall: get_LocalPolicy failed: 0x%08lx\n", hr);
		return false;
    }

    // Retrieve the firewall profile currently in effect.
    hr = fwPolicy->get_CurrentProfile(&fwProf);
    if (FAILED(hr))
    {
		// Sometimes, this fails at boot time. So, we 
		// retry five times before throwing in the towel.
		int i;
		for (i=0; i<5; i++) {
    		hr = fwPolicy->get_CurrentProfile(&fwProf);
    		if (SUCCEEDED(hr)) {
				break;
			} else {
				dprintf(D_FULLDEBUG, "get_CurrentProfile() failed. "
					   " Retry %d...\n", i);
				sleep(1);
			}
		}

		if ( FAILED(hr) ) {
	        dprintf(D_ALWAYS, 
					"WinFirewall: get_CurrentProfile failed: 0x%08lx\n", hr);
			return false;
		}
    }

    // Retrieve the authorized application collection.
    hr = fwProf->get_AuthorizedApplications(&fwApps);
    if (FAILED(hr))
    {
        dprintf(D_ALWAYS, 
			"WinFirewall: get_AuthorizedApplications failed: 0x%08lx\n", hr);
        return false;
    }

    return true;
}

void
WindowsFirewallHelper::WindowsFirewallCleanup() {

	// Release the local firewall policy.
    if (fwPolicy != NULL) {
        fwPolicy->Release();
		fwPolicy = NULL;
    }

	// Release the firewall settings manager.
    if (fwMgr != NULL) {
	   	fwMgr->Release();
		fwMgr = NULL;
   	}

	if (fwProf != NULL)
    {
        fwProf->Release();
		fwProf = NULL;
    }

	wfh_initialized = false;

}

#ifdef _WFW_HELPER_TESTING

/* some pretty lame, but useful test code */

int main(int argc, char **argv) {
	WindowsFirewallHelper wfh;
	int result;
	bool isOn;
	const char* app;

	result = 0;

	Termlog = 1;
	dprintf_config ("TOOL", 2 );

	isOn = wfh.firewallIsOn();

	printf("firewall is %s.\n", (isOn) ? "On" : "Off");

	if ( argc <= 1 ) {
		app = "C:\\Condor\\bin\\condor_master.exe";
	} else {
		app = argv[1];
	}

	isOn = wfh.applicationIsTrusted(app);

	printf("%s is %s by the firewall.\n", app, (isOn) ? "Trusted" : "Not Trusted");

	wfh.removeTrusted(app);
	// wfh.addTrusted(app);

	if ( ! wfh.addTrusted("C:\\Condor\\bin\\condor_master.exe") ) {
		printf("first addTrusted() failed\n");
		result = 1;
	}

	if ( ! wfh.addTrusted("C:\\Condor\\condor_master.exe") ) {
		printf("second addTrusted() failed\n");
		result = 1;
	}

	if ( ! wfh.removeTrusted("C:\\Condor\\bin\\condor_master.exe") ) {
		printf("first removeTrusted() failed\n");
		result = 1;
	}

	if ( ! wfh.removeTrusted("C:\\Condor\\bin\\condor_master.exe") ) {
		printf("second removeTrusted() failed\n");
		result = 1;
	}

	printf("tests are done!\n");

	return result;
}

#endif /* _WFW_HELPER_TESTING */

#endif /* WIN32 */
