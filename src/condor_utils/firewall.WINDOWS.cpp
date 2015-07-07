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




#include "condor_common.h"
#ifdef WIN32
#include "condor_config.h"
#include "firewall.WINDOWS.h"
#include "basename.h"

static const char * GetHRString(HRESULT hr)
{
	switch (hr) {
	case S_OK: return "Ok";
	case E_ACCESSDENIED: return "Access Denied";
	case E_NOTIMPL: return "Not implemented";
	case E_INVALIDARG: return "Invalid Argument";
	case E_OUTOFMEMORY: return "Out of memory";
	case E_NOINTERFACE: return "No such Interface";
	case E_POINTER: return "Invalid pointer";
	case E_HANDLE: return "Invalid handle";
	case E_ABORT: return "Operation aborted";
	}
	return "";
}


WindowsFirewallHelper::WindowsFirewallHelper()
	: fwProf(NULL)
	, fwMgr(NULL)
	, fwApps(NULL)
	, fwPolicy(NULL)
	, wfh_initialized(false)
	, wfh_operational(false)
	, i_can_add(true)
	, i_can_remove(true)
{
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
        dprintf(D_ALWAYS, "WinFirewall: CoInitializeEx failed: 0x%08lx %s\n", hr, GetHRString(hr));
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
        dprintf(D_ALWAYS, "WinFirewall: get_FirewallEnabled failed: 0x%08lx %s\n", hr, GetHRString(hr));
        return false;
    }

    return (fwEnabled == VARIANT_TRUE);    
}

HRESULT
WindowsFirewallHelper::addTrusted( const char *app_path ) {

	const char *app_basename;
	BSTR app_path_bstr = NULL;
	BSTR app_basename_bstr = NULL;

	HRESULT hr = S_OK;
	INetFwAuthorizedApplication* fwApp = NULL;
	
	if ( ! ready() || ! i_can_add) { 
		return S_FALSE;
	}

	if ( !firewallIsOn() ) {
		// firewall is turned off, so there's nothing to do.
		return S_FALSE;
	}

	if ( applicationIsTrusted(app_path) ) {
		// this app is already set to be trusted, so do nothing.
		return S_OK;
	}

	// now, if the basename of the app is condor_<something>, we 
	// want to make sure there aren't any other entries of the same
	// condor daemon with a different path. We only do this for "condor_" 
	// executables as a safety to keep us from removing trusted applications
	// that have nothing to do with condor.
	app_basename = condor_basename(app_path);

	if ( _strnicmp(app_basename, "condor_", strlen("condor_")) == 0 ) {
		
		hr = removeByBasename(app_basename);

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
		i_can_add = false;
		dprintf(D_ERROR | D_FULLDEBUG, "WinFirewall: CoCreateInstance failed: 0x%08lx %s\n", hr, GetHRString(hr));
		return hr;
	}
	
	app_path_bstr = charToBstr(app_path);
	// Set the process image file name.
	hr = fwApp->put_ProcessImageFileName(app_path_bstr);
	if (FAILED(hr))
	{
		if ( hr == 0x80070002 ) {
			dprintf(D_ERROR, "WinFirewall Error: Could not find trusted app image %s\n",
				app_path);
		} else {
			dprintf(D_ERROR, "put_ProcessImageFileName failed: 0x%08lx %s\n", hr, GetHRString(hr));
		}
		goto error;
	}
	
        // Allocate a BSTR for the application friendly name.
        app_basename_bstr = charToBstr(app_basename);

        // Set the application friendly name.
        hr = fwApp->put_Name(app_basename_bstr);
        if (FAILED(hr))
        {
            dprintf(D_ERROR | D_FULLDEBUG, "WinFirewall: put_Name failed: 0x%08lx %s\n", hr, GetHRString(hr));
            goto error;
        }

        // Add the application to the collection.
        hr = fwApps->Add(fwApp);
        if (FAILED(hr))
        {
            dprintf(D_ERROR, "WinFirewall: Add failed: 0x%08lx %s\n", hr, GetHRString(hr));
            goto error;
        }

		// it seems like we should always inform users somehow that we're 
		// doing this.
        dprintf(D_STATUS, "Authorized application %s is now enabled in the"
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

	if (hr == E_ACCESSDENIED) i_can_add = false;
    return hr;

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
            dprintf(D_ALWAYS, "WinFirewall: get_Enabled failed: 0x%08lx %s\n", hr, GetHRString(hr));
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

HRESULT
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
		return S_FALSE;
	}

	hr = fwApps->get__NewEnum(&pUnknown);
	if ( hr != S_OK ) {
		dprintf(D_ERROR, "Failed to get enumerator for Authorized "
				"Applications (err=%x)\n", hr);
		return hr;
	}

	hr = fwApps->get_Count(&count);
	if ( hr != S_OK ) {
		dprintf(D_ERROR, "Failed to get count of Authorized "
				"Applications (err=%x)\n", hr);
		return hr;
	}

	hr = pUnknown->QueryInterface(IID_IEnumVARIANT, (void**)&pEnum);
	if ( hr != S_OK ) {
		if ( hr == E_NOINTERFACE ) {
			dprintf(D_ERROR, "Failed to QueryInterface for trusted "
					"applications. Interface not supported.\n");
		} else {
			dprintf(D_ERROR, "Failed to QueryInterface for trusted "
				   "applications. (err=%x)\n", hr);
		}
		return hr;
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
			hr = S_FALSE;
			break;
		}

		fwApp = (INetFwAuthorizedApplication*)v.byref;

		fwApp->get_ProcessImageFileName(&str);

		// This should not be printing to stdout, but if you are
		// reading this and think it is necessary to print these 
		// executable names then dprintf() them.
		// printf("Result is %lS\n", str);

		len = wcslen(str);
		tmp = (char*) malloc((len*2+1) * sizeof(char));
		ASSERT(tmp);
		sprintf(tmp, "%S", str);

		bn = condor_basename(tmp);

		if ( 0 == strcasecmp(bn, name) ) {
			
			// basenames match, so remove it from the list.
			
			hr = removeTrusted(tmp);
		}
		free(tmp);

		SysFreeString(str);

		if (hr == E_ACCESSDENIED) {
			// don't have enough privilege to remove, so don't bother to keep trying.
			break;
		}
	}

	return hr;
}

HRESULT
WindowsFirewallHelper::removeTrusted( const char *app_path) {
	
	BSTR app_path_bstr = NULL;

	if ( ! ready() || ! i_can_remove) {
		return S_FALSE;
	}

	app_path_bstr = charToBstr(app_path);
	if ( app_path_bstr == NULL ) {
		return S_FALSE;
	}

	// Attempt to retrieve the authorized application.
	HRESULT hr = fwApps->Remove(app_path_bstr);
	if (FAILED(hr)) {
		dprintf(D_ERROR, "WinFirewall: remove trusted app %s failed: 0x%08lx %s\n", app_path, hr, GetHRString(hr));
		if (hr == E_ACCESSDENIED) {
			i_can_remove = false;
		}
	}

    // Free the BSTR.
    SysFreeString(app_path_bstr);

	return hr;
}

BSTR
WindowsFirewallHelper::charToBstr(const char* str) {

	BSTR the_bstr;
	HRESULT hr = S_OK;

	int cch = MultiByteToWideChar(CP_ACP, 0, str, strlen(str), NULL, 0);
	the_bstr = SysAllocStringLen(NULL, cch); // SysAllocateString adds +1 to allocation size
	if ( ! the_bstr) {
        hr = E_OUTOFMEMORY;
        dprintf(D_ERROR, "WinFirewall: SysAllocString failed\n");
        return NULL;
	}
	MultiByteToWideChar(CP_ACP, 0, str, -1, (WCHAR*)the_bstr, cch+1);
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
				"WinFirewall: CoCreateInstance failed: 0x%08lx %s\n", hr, GetHRString(hr));
		return false;
    }

    // Retrieve the local firewall policy.
    hr = fwMgr->get_LocalPolicy(&fwPolicy);
    if (FAILED(hr))
    {
        dprintf(D_ALWAYS, "WinFirewall: get_LocalPolicy failed: 0x%08lx %s\n", hr, GetHRString(hr));
		return false;
    }

    // Retrieve the firewall profile currently in effect.
    hr = fwPolicy->get_CurrentProfile(&fwProf);
    if (FAILED(hr))
    {
		const char * err_string = "";

		// Sometimes, this fails at boot time. So, we 
		// retry a number of times before throwing in the towel.
		// Note that if retry = 0, it retries forever.
		int retry = param_integer("WINDOWS_FIREWALL_FAILURE_RETRY", 2);
		int i;
		for (i=0; (i<retry || (retry==0)); i++) {
    		hr = fwPolicy->get_CurrentProfile(&fwProf);
    		if (SUCCEEDED(hr)) {
				break;
			} else if (hr == E_ACCESSDENIED || hr == E_NOTIMPL || hr == E_INVALIDARG) {
				// don't bother to retry.
			} else {
				dprintf(D_FULLDEBUG, "WinFirewall: get_CurrentProfile() "
					   "failed.  Retry %d...\n", i);
				sleep(5); // 5 seconds
			}
		}

		if ( FAILED(hr) ) {
	        dprintf(D_ALWAYS, 
					"WinFirewall: get_CurrentProfile failed: 0x%08lx %s\n", hr, GetHRString(hr));
			return false;
		}
    }

    // Retrieve the authorized application collection.
    hr = fwProf->get_AuthorizedApplications(&fwApps);
    if (FAILED(hr))
    {
        dprintf(D_ALWAYS, 
			"WinFirewall: get_AuthorizedApplications failed: 0x%08lx %s\n", hr, GetHRString(hr));
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

	dprintf_set_tool_debug("TOOL", 0);

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

	if ( ! SUCCEEDED(wfh.addTrusted("C:\\Condor\\bin\\condor_master.exe")) ) {
		printf("first addTrusted() failed\n");
		result = 1;
	}

	if ( ! SUCCEEDED(wfh.addTrusted("C:\\Condor\\condor_master.exe")) ) {
		printf("second addTrusted() failed\n");
		result = 1;
	}

	HRESULT hr = wfh.removeTrusted("C:\\Condor\\bin\\condor_master.exe");
	if (FAILED(hr)) {
		printf("first removeTrusted() failed 0x%08x\n", hr);
		result = 1;
	}


	hr = wfh.removeTrusted("C:\\Condor\\bin\\condor_master.exe");
	if (FAILED(hr)) {
		printf("second removeTrusted() failed 0x%08x\n", hr);
		result = 1;
	}

	printf("tests are done!\n");

	return result;
}

#endif /* _WFW_HELPER_TESTING */

#endif /* WIN32 */
