/***************************************************************
 *
 * Copyright (C) 2020, Condor Team, Computer Sciences Department,
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


#ifndef __CONDOR_DC_VPN_H__
#define __CONDOR_DC_VPN_H__

#include "condor_common.h"
#include "condor_classad.h"
#include "condor_io.h"
#include "enum_utils.h"
#include "daemon.h"

class DCVPN : public Daemon {
public:

	DCVPN(const char* const name = nullptr, const char* pool = nullptr);
	~DCVPN();

	bool registerClient(const std::string &base64_pubkey,
				std::string &ipaddr,
				std::string &netmask,
				std::string &network,
				std::string &gwaddr,
				std::string &base64_server_pubkey,
				std::string &server_endpoint,
				CondorError &err);
};
#endif /* _CONDOR_DC_VPN_H */
