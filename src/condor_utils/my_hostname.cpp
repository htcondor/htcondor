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
#include "condor_netdb.h"
#include "ipv6_hostname.h"
#include "condor_sinful.h"

#include "subsystem_info.h"

static bool enable_convert_default_IP_to_socket_IP = false;
static std::set< std::string > configured_network_interface_ips;
static bool network_interface_matches_all;


const char* my_ip_string() {
    static MyString __my_ip_string;
	// TODO: Picking IPv4 arbitrarily. WARNING: This function
	// gets called while the configuration file is being loaded,
	// before we know if IPV4 and/or IPv6 is enabled.  It needs to
	// return a stable answer, because having it change midway
	// through parsing the file is a recipe for failure.
    __my_ip_string = get_local_ipaddr(CP_IPV4).to_ip_string();
    return __my_ip_string.Value();
}

//void
//init_full_hostname()
//{
//	char *tmp;
//
//	tmp = get_full_hostname( hostname );
//
//	if( full_hostname ) {
//		free( full_hostname );
//	}
//	if( tmp ) {
//			// Found it, use it.
//		full_hostname = strdup( tmp );
//		delete [] tmp;
//	} else {
//			// Couldn't find it, just use what we've already got.
//		full_hostname = strdup( hostname );
//	}
//}

bool
network_interface_to_ip(char const *interface_param_name,char const *interface_pattern,std::string & ipv4, std::string & ipv6, std::string & ipbest, std::set< std::string > *network_interface_ips)
{
	ASSERT( interface_pattern );
	if( !interface_param_name ) {
		interface_param_name = "";
	}

	if( network_interface_ips ) {
		network_interface_ips->clear();
	}

	condor_sockaddr addr;
	if (addr.from_ip_string(interface_pattern)) {
		if(addr.is_ipv4()) {
			ipv4 = interface_pattern;
		} else {
			ASSERT(addr.is_ipv6());
			ipv6 = interface_pattern;
		}
		if( network_interface_ips ) {
			network_interface_ips->insert( interface_pattern );
		}

		dprintf(D_HOSTNAME,"%s=%s, so choosing IP %s\n",
				interface_param_name,
				interface_pattern,
				addr.to_ip_string().Value());

		return true;
	}

	StringList pattern(interface_pattern);

	std::string matches_str;
	std::vector<NetworkDeviceInfo> dev_list;
	std::vector<NetworkDeviceInfo>::iterator dev;

	bool want_v4 = param_boolean("ENABLE_IPV4", true);
	bool want_v6 = param_boolean("ENABLE_IPV6", true);
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
		bool matches = false;
		if( strcmp(dev->name(),"")!=0 &&
			pattern.contains_anycase_withwildcard(dev->name()) )
		{
			matches = true;
		}
		else if( strcmp(dev->IP(),"")!=0 &&
				 pattern.contains_anycase_withwildcard(dev->IP()) )
		{
			matches = true;
		}

		if( !matches ) {
			dprintf(D_HOSTNAME,"Ignoring network interface %s (%s) because it does not match %s=%s.\n",
					dev->name(), dev->IP(), interface_param_name, interface_pattern);
			continue;
		}

		condor_sockaddr this_addr;
		if (!this_addr.from_ip_string(dev->IP())) {
			dprintf(D_HOSTNAME,"Ignoring network interface %s (%s) because it does not have a useable IP address.\n",
					dev->name(), dev->IP());
			continue;
		}

		if( matches_str.size() ) {
			matches_str += ", ";
		}
		matches_str += dev->name();
		matches_str += " ";
		matches_str += dev->IP();

		if( network_interface_ips ) {
			network_interface_ips->insert( dev->IP() );
		}

		int desireability = this_addr.desirability();
		if(dev->is_up()) { desireability *= 10; }

		int * best_so_far = 0;
		std::string * ip = 0;
		if(this_addr.is_ipv4()) {
			best_so_far = & best_so_far_v4;
			ip = & ipv4;
		} else {
			ASSERT(this_addr.is_ipv6());
			best_so_far = & best_so_far_v6;
			ip = & ipv6;
		}

		//dprintf(D_HOSTNAME, "Considering %s (Ranked at %d) as possible local hostname versus %s (%d)\n", addr.to_ip_string().Value(), desireability, ip.c_str(), desireability);

		if( desireability > *best_so_far ) {
			*best_so_far = desireability;
			*ip = dev->IP();
		}

		if( desireability > best_overall ) {
			best_overall = desireability;
			ipbest = dev->IP();
		}

	}

	if( best_overall < 0 ) {
		dprintf(D_ALWAYS,"Failed to convert %s=%s to an IP address.\n",
				interface_param_name ? interface_param_name : "",
				interface_pattern);
		return false;
	}

	dprintf(D_HOSTNAME,"%s=%s matches %s, choosing IP %s\n",
			interface_param_name,
			interface_pattern,
			matches_str.c_str(),
			ipbest.c_str());

	return true;
}

void
init_network_interfaces( int config_done )
{
	dprintf( D_HOSTNAME, "Trying to getting network interface informations (%s)\n",
		 config_done ? "after reading config" : "config file not read" );

	std::string network_interface;

	if( config_done ) {
		param(network_interface,"NETWORK_INTERFACE");
	}
	if( network_interface.empty() ) {
		network_interface = "*";
	}

	network_interface_matches_all = (network_interface == "*");

	std::string network_interface_ipv4;
	std::string network_interface_ipv6;
	std::string network_interface_best;
	bool ok;
	ok = network_interface_to_ip(
		"NETWORK_INTERFACE",
		network_interface.c_str(),
		network_interface_ipv4,
		network_interface_ipv6,
		network_interface_best,
		&configured_network_interface_ips);

	if( !ok ) {
		EXCEPT("Failed to determine my IP address using NETWORK_INTERFACE=%s",
			   network_interface.c_str());
	}
}

#ifdef WIN32
	// see below comment in init_hostname() to learn why we must
	// include condor_io in this module.
#include "condor_io.h"
#endif

void ConfigConvertDefaultIPToSocketIP()
{
	enable_convert_default_IP_to_socket_IP = true;

	/*
	  When using TCP_FORWARDING_HOST, if we rewrite addresses, we will
	  insert the IP address of the local IP address in place of
	  the forwarding IP address.
	*/
	char *str = param("TCP_FORWARDING_HOST");
	if( str && *str ) {
		enable_convert_default_IP_to_socket_IP = false;
		dprintf(D_FULLDEBUG,"Disabling ConvertDefaultIPToSocketIP() because TCP_FORWARDING_HOST is defined.\n");
	}
	free( str );

	if( configured_network_interface_ips.size() <= 1 ) {
		enable_convert_default_IP_to_socket_IP = false;
		dprintf(D_FULLDEBUG,"Disabling ConvertDefaultIPToSocketIP() because NETWORK_INTERFACE does not match multiple IPs.\n");
	}

	if( !param_boolean("ENABLE_ADDRESS_REWRITING",true) ) {
		enable_convert_default_IP_to_socket_IP = false;
		dprintf(D_FULLDEBUG,"Disabling ConvertDefaultIPToSocketIP() because ENABLE_ADDRESS_REWRITING is false.\n");
	}
}
