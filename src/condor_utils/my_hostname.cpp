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
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/



#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "internet.h"
#include "my_hostname.h"
#include "condor_attributes.h"
#include "ipv6_hostname.h"
#include "condor_sinful.h"
#include "CondorError.h"
#include "../condor_sysapi/sysapi.h"

bool
network_interface_to_sockaddr(char const *interface_param_name, char const *interface_pattern, condor_sockaddr & ipv4, condor_sockaddr & ipv6, condor_sockaddr & ipbest)
{
	ASSERT( interface_pattern );
	if( !interface_param_name ) {
		interface_param_name = "";
	}

	// If the pattern is a literal IP address, just use it.
	// Exception: If it's an IPv6 link-local address, we need to get the
	//   scope-id from the OS.
	// Note: Do we want to blindly use an IP address that may not be
	//   present on the local system?
	condor_sockaddr addr;
	if (addr.from_ip_string(interface_pattern) && (addr.is_ipv4() || !addr.is_link_local())) {
		if(addr.is_ipv4()) {
			ipv4 = addr;
			ipbest = ipv4;
			ipv6.clear();
		} else {
			ASSERT(addr.is_ipv6());
			ipv6 = addr;
			ipbest = ipv6;
			ipv4.clear();
		}

		dprintf(D_HOSTNAME,"%s=%s, so choosing IP %s\n",
				interface_param_name,
				interface_pattern,
				addr.to_ip_string().c_str());

		return true;
	}

	ipv4.clear();
	ipv6.clear();
	ipbest.clear();

	std::vector<std::string> pattern = split(interface_pattern);

	std::string matches_str;
	std::vector<NetworkDeviceInfo> dev_list;
	std::vector<NetworkDeviceInfo>::iterator dev;

	bool want_v4 = ! param_false( "ENABLE_IPV4" );
	bool want_v6 = ! param_false( "ENABLE_IPV6" );
	sysapi_get_network_device_info(dev_list, want_v4, want_v6);

		// Order of preference:
		//   * non-private IP
		//   * private IP (e.g. 192.168.*)
		//   * loopback
		// In case of a tie, choose the first device in the list.

	int best_so_far_v4 = -1;
	int best_so_far_v6 = -1;
	int best_overall = -1;

	for(dev = dev_list.begin();
		dev != dev_list.end();
		dev++)
	{
		std::string ip_str = dev->addr().to_ip_string();
		bool matches = false;
		if( strcmp(dev->name(),"")!=0 &&
			contains_anycase_withwildcard(pattern, dev->name()) )
		{
			matches = true;
		}
		else if( !ip_str.empty() &&
				 contains_anycase_withwildcard(pattern, ip_str) )
		{
			matches = true;
		}

		if( !matches ) {
			dprintf(D_HOSTNAME,"Ignoring network interface %s (%s) because it does not match %s=%s.\n",
					dev->name(), ip_str.c_str(), interface_param_name, interface_pattern);
			continue;
		}

		if( matches_str.size() ) {
			matches_str += ", ";
		}
		matches_str += dev->name();
		matches_str += " ";
		matches_str += ip_str;

		int desireability = dev->addr().desirability();
		if(dev->is_up()) { desireability *= 10; }

		int * best_so_far = 0;
		condor_sockaddr * ip = 0;
		if(dev->addr().is_ipv4()) {
			best_so_far = & best_so_far_v4;
			ip = & ipv4;
		} else {
			ASSERT(dev->addr().is_ipv6());
			best_so_far = & best_so_far_v6;
			ip = & ipv6;
		}

		//dprintf(D_HOSTNAME, "Considering %s (Ranked at %d) as possible local hostname versus %s (%d)\n", addr.to_ip_string().Value(), desireability, ip.c_str(), desireability);

		if( desireability > *best_so_far ) {
			*best_so_far = desireability;
			*ip = dev->addr();
		}

		if( desireability > best_overall ) {
			best_overall = desireability;
			ipbest = dev->addr();
		}
	}

	if( best_overall < 0 ) {
		dprintf(D_ALWAYS,"Failed to convert %s=%s to an IP address.\n",
				interface_param_name, interface_pattern);
		return false;
	}

	//
	// Add some smarts to ENABLE_IPV[4|6] = AUTO.
	//
	// If we only found one protocol, do nothing.  Otherwise,
	// if both ipv4 and ipv6 are not at least as desirable as a private
	// address, then do nothing.  If both are at least as desirable as a
	// private address, do nothing.  If only one is, and that ENABLE
	// knob is AUTO, clear the corresponding ipv[4|6] variable.
	//
	// We're using the raw desirability parameter here, but we should maybe
	// be checking to see if the address is public or private instead...
	//
	if( ipv4.is_valid() && ipv6.is_valid() ) {
		if( (ipv4.desirability() < 4) ^ (ipv6.desirability() < 4) ) {
			if( want_v4 && ! param_true( "ENABLE_IPV4" ) ) {
				if( ipv4.desirability() < 4 ) {
					ipv4.clear();
					ipbest = ipv6;
				}
			}
			if( want_v6 && ! param_true( "ENABLE_IPV6" ) ) {
				if( ipv6.desirability() < 4) {
					ipv6.clear();
					ipbest = ipv4;
				}
			}
		}
	}

	dprintf(D_HOSTNAME,"%s=%s matches %s, choosing IP %s\n",
			interface_param_name,
			interface_pattern,
			matches_str.c_str(),
			ipbest.to_ip_string().c_str());

	return true;
}


bool
validate_network_interfaces( CondorError * errorStack )
{
	dprintf( D_HOSTNAME, "Trying to getting network interface information after reading config\n" );

	bool enable_ipv4_true = false;
	bool enable_ipv4_false = false;
	bool enable_ipv6_true = false;
	bool enable_ipv6_false = false;
	std::string enable_ipv4_str;
	std::string enable_ipv6_str;
	param( enable_ipv4_str, "ENABLE_IPV4" );
	param( enable_ipv6_str, "ENABLE_IPV6" );
	bool result = false;
	if ( string_is_boolean_param( enable_ipv4_str.c_str(), result ) ) {
		enable_ipv4_true = result;
		enable_ipv4_false = !result;
	}
	if ( string_is_boolean_param( enable_ipv6_str.c_str(), result ) ) {
		enable_ipv6_true = result;
		enable_ipv6_false = !result;
	}

	std::string network_interface;
	param( network_interface, "NETWORK_INTERFACE" );

	if( enable_ipv4_false && enable_ipv6_false ) {
		errorStack->pushf( "validate_network_interfaces", 1, "ENABLE_IPV4 and ENABLE_IPV6 are both false." );
		return false;
	}

	condor_sockaddr network_interface_ipv4;
	condor_sockaddr network_interface_ipv6;
	condor_sockaddr network_interface_best;
	bool ok;
	ok = network_interface_to_sockaddr(
		"NETWORK_INTERFACE",
		network_interface.c_str(),
		network_interface_ipv4,
		network_interface_ipv6,
		network_interface_best);

	if( !ok ) {
		errorStack->pushf( "validate_network_interfaces", 2,
				"Failed to determine my IP address using NETWORK_INTERFACE=%s",
				network_interface.c_str());
		return false;
	}

	//
	// Check the validity of the configuration.
	//
	if( !network_interface_ipv4.is_valid() && enable_ipv4_true ) {
		errorStack->pushf( "validate_network_interfaces", 3, "ENABLE_IPV4 is TRUE, but no IPv4 address was detected.  Ensure that your NETWORK_INTERFACE parameter is not set to an IPv6 address." );
		return false;
	}
	// We don't have an enum type in the param system (yet), so check.
	if( !enable_ipv4_true && !enable_ipv4_false ) {
		if( strcasecmp( enable_ipv4_str.c_str(), "AUTO" ) ) {
			errorStack->pushf( "validate_network_interfaces", 4, "ENABLE_IPV4 is '%s', must be 'true', 'false', or 'auto'.", enable_ipv4_str.c_str() );
			return false;
		}
	}

	if( !network_interface_ipv6.is_valid() && enable_ipv6_true ) {
		errorStack->pushf( "validate_network_interfaces", 5, "ENABLE_IPV6 is TRUE, but no IPv6 address was detected.  Ensure that your NETWORK_INTERFACE parameter is not set to an IPv4 address." );
		return false;
	}
	// We don't have an enum type in the param system (yet), so check.
	if( !enable_ipv6_true && !enable_ipv6_false ) {
		if( strcasecmp( enable_ipv6_str.c_str(), "AUTO" ) ) {
			errorStack->pushf( "validate_network_interfaces", 6, "ENABLE_IPV6 is '%s', must be 'true', 'false', or 'auto'.", enable_ipv6_str.c_str() );
			return false;
		}
	}

	if( (network_interface_ipv4.is_valid()) && enable_ipv4_false ) {
		errorStack->pushf( "validate_network_interfaces", 7, "ENABLE_IPV4 is false, yet we found an IPv4 address.  Ensure that NETWORK_INTERFACE is set appropriately." );
		return false;
	}

	if( (network_interface_ipv6.is_valid()) && enable_ipv6_false ) {
		errorStack->pushf( "validate_network_interfaces", 8, "ENABLE_IPV6 is false, yet we found an IPv6 address.  Ensure that NETWORK_INTERFACE is set appropriately." );
		return false;
	}

	return true;
}
