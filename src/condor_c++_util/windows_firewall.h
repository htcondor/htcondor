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
