/***************************************************************
 *
 * Copyright (C) 2021, Condor Team, Computer Sciences Department,
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
#include "condor_debug.h"
#include "condor_classad.h"
#include "condor_commands.h"
#include "condor_attributes.h"
#include "daemon.h"
#include "dc_vpn.h"

// // // // //
// DCVPN
// // // // //

DCVPN::DCVPN(const char* name, const char* pool) 
	: Daemon(DT_VPN, name, pool)
{
}


DCVPN::~DCVPN()
{
}


bool
DCVPN::registerClient(const std::string &base64_pubkey,
	std::string &ipaddr,
	std::string &netmask,
	std::string &network,
	std::string &gwaddr,
	std::string &base64_server_pubkey,
	std::string &server_endpoint,
	CondorError &err)
{
	std::unique_ptr<ReliSock>rsock_ptr;
	rsock_ptr.reset((ReliSock *)startCommand(
			VPN_REGISTER, Stream::reli_sock, 20, &err));
	if (!rsock_ptr) {
		err.push("DC_VPN", 1,
			"Failed to connect to remote VPN server.");
		return false;
	}
	ReliSock &rsock = *rsock_ptr.get();

	ClassAd reqAd;
	reqAd.InsertAttr("ClientPubkey", base64_pubkey);

	rsock.encode();
	if (!putClassAd(&rsock, reqAd) || !rsock.end_of_message()) {
		err.push("DC_VPN", 2,
			"Failed to send client request to VPN server.");
		return false;
	}

		// Receive the return code
	rsock.decode();
	ClassAd respAd;
	if (!getClassAd(&rsock, respAd) || !rsock.end_of_message()) {
		err.push("DC_VPN", 3,
			"Failed to get registration response from VPN server.");
		return false;
	}
	rsock.close();

	dPrintAd(D_FULLDEBUG, respAd);

	int error;
	if (respAd.EvaluateAttrInt(ATTR_ERROR_CODE, error)) {
		std::string error_string = "(Unknown)";
		respAd.EvaluateAttrString(ATTR_ERROR_STRING, error_string);
		err.push("DC_VPN", error, error_string.c_str());
		return false;
	}

	if (!respAd.EvaluateAttrString("IpAddr", ipaddr)) {
		err.push("DC_VPN", 4,
			"Server response did not include IP address.");
		return false;
	}
	if (!respAd.EvaluateAttrString("GW", gwaddr)) {
		err.push("DC_VPN", 5,
			"Server response did not include gateway address.");
		return false;
	}
	if (!respAd.EvaluateAttrString("Network", network)) {
		err.push("DC_VPN", 6,
			"Server response did not include network address.");
		return false;
	}
	if (!respAd.EvaluateAttrString("Netmask", netmask)) {
		err.push("DC_VPN", 7,
			"Server response did not include network netmask.");
		return false;
	}
	if (!respAd.EvaluateAttrString("Pubkey", base64_server_pubkey)) {
		err.push("DC_VPN", 7,
			"Server response did not include pubkey.");
		return false;
	}
	if (!respAd.EvaluateAttrString("Endpoint", server_endpoint)) {
		err.push("DC_VPN", 8,
			"Server response did not include endpoint.");
		return false;
	}

	return true;
}
