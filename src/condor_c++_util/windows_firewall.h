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


#ifdef WIN32

#if !defined(_CONDOR_WINDOWS_FIREWALL_H)
#define _CONDOR_WINDOWS_FIREWALL_H

/* bring in the DCOM junk. Hooray! */
#define _WIN32_DCOM

#include <objbase.h>
#include <netfw.h>
#include <unknwn.h>

/*
 This is a convenience class for manipulating the Windows XP SP2 firewall.
 The main goal of this class is to provide a simple API for cleanly
 adding condor daemons to the list of "trusted" applications to the
 windows firewall. Applications in this trusted list can use the network
 without being interfered with by the windows firewall.

 Note that special care has been taken to:

 1. Not change anything if the firewall is disabled.
 2. Not change anything if the firewall already has the daemon in
 	question on the "trusted" list".
 3. Not add multiple instances of the same condor daemon (with
	different absolute paths) to the trusted list. If the same daemon
	under a different path exists on the list, we remove the old
	entry and add a new entry with the path we currently have.

*/


class WindowsFirewallHelper
{
public:

	WindowsFirewallHelper();
	~WindowsFirewallHelper();

	bool init();

	bool ready();
	
	bool firewallIsOn();
	bool applicationIsTrusted(const char* app_path);
	
	bool addTrusted(const char* app_path);
	bool removeTrusted(const char* app_path);
	
private:

	bool WindowsFirewallInitialize();
	void WindowsFirewallCleanup();

	bool removeByBasename(const char* basename);
	
	/* data members */

	INetFwProfile *fwProf;
	INetFwMgr *fwMgr;
	INetFwAuthorizedApplications *fwApps;
    INetFwPolicy *fwPolicy;

	BSTR charToBstr(const char*);
	
	bool wfh_initialized; // flag indicating init() has been called.
	bool wfh_operational; // flag indicating whether this OS supports ICF
};

#endif /* _CONDOR_WINDOWS_FIREWALL_H */

#endif /* WIN32 */
