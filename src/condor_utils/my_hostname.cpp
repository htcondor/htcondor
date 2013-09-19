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
#include "condor_debug.h"
#include "condor_config.h"
#include "internet.h"
#include "my_hostname.h"
#include "condor_attributes.h"
#include "condor_netdb.h"
#include "ipv6_hostname.h"

static bool enable_convert_default_IP_to_socket_IP = true;
static std::set< std::string > configured_network_interface_ips;
static bool network_interface_matches_all;

//static void init_hostnames();

// Return our hostname in a static data buffer.
const char *
my_hostname()
{
//	if( ! hostnames_initialized ) {
//		init_hostnames();
//	}
//	return hostname;
    static MyString __my_hostname;
    __my_hostname = get_local_hostname();
    return __my_hostname.Value();
}


// Return our full hostname (with domain) in a static data buffer.
const char* my_full_hostname() {
    static MyString __my_full_hostname;
    __my_full_hostname = get_local_fqdn();
    return __my_full_hostname.Value();
}

const char* my_ip_string() {
    static MyString __my_ip_string;
    __my_ip_string = get_local_ipaddr().to_ip_string();
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
network_interface_to_ip(char const *interface_param_name,char const *interface_pattern,std::string &ip,std::set< std::string > *network_interface_ips)
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
		ip = interface_pattern;
		if( network_interface_ips ) {
			network_interface_ips->insert( ip );
		}

		dprintf(D_HOSTNAME,"%s=%s, so choosing IP %s\n",
				interface_param_name,
				interface_pattern,
				ip.c_str());

		return true;
	}

	StringList pattern(interface_pattern);

	std::string matches_str;
	std::vector<NetworkDeviceInfo> dev_list;
	std::vector<NetworkDeviceInfo>::iterator dev;

	sysapi_get_network_device_info(dev_list);

		// Order of preference:
		//   * non-private IP
		//   * private IP (e.g. 192.168.*)
		//   * loopback
		// In case of a tie, choose the first device in the list.

	int best_so_far = -1;

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

		int desireability;

		if (this_addr.is_loopback()) {
			desireability = 1;
		}
		else if (this_addr.is_private_network()) {
			desireability = 2;
		}
		else {
			desireability = 3;
		}

		if(dev->is_up()) { desireability *= 10; }

		//dprintf(D_HOSTNAME, "Considering %s (Ranked at %d) as possible local hostname versus %s (%d)\n", addr.to_ip_string().Value(), desireability, ip.c_str(), desireability);

		if( desireability > best_so_far ) {
			best_so_far = desireability;
			ip = dev->IP();
		}
	}

	if( best_so_far < 0 ) {
		dprintf(D_ALWAYS,"Failed to convert %s=%s to an IP address.\n",
				interface_param_name ? interface_param_name : "",
				interface_pattern);
		return false;
	}

	dprintf(D_HOSTNAME,"%s=%s matches %s, choosing IP %s\n",
			interface_param_name,
			interface_pattern,
			matches_str.c_str(),
			ip.c_str());

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

	std::string network_interface_ip;
	bool ok;
	ok = network_interface_to_ip(
		"NETWORK_INTERFACE",
		network_interface.c_str(),
		network_interface_ip,
		&configured_network_interface_ips);

	if( !ok ) {
		EXCEPT("Failed to determine my IP address using NETWORK_INTERFACE=%s",
			   network_interface.c_str());
	}
}

//void
//init_ipaddr( int config_done )
//{
//    if( ! hostname ) {
//		init_hostnames();
//	}
//
//	dprintf( D_HOSTNAME, "Trying to initialize local IP address (%s)\n",
//		 config_done ? "after reading config" : "config file not read" );
//
//	std::string network_interface;
//
//	if( config_done ) {
//		param(network_interface,"NETWORK_INTERFACE");
//	}
//	if( network_interface.empty() ) {
//		network_interface = "*";
//	}
//
//	network_interface_matches_all = (network_interface == "*");
//
//	std::string network_interface_ip;
//	bool ok;
//	ok = network_interface_to_ip(
//		"NETWORK_INTERFACE",
//		network_interface.c_str(),
//		network_interface_ip,
//		&configured_network_interface_ips);
//
//	if( !ok ) {
//		EXCEPT("Failed to determine my IP address using NETWORK_INTERFACE=%s",
//			   network_interface.c_str());
//	}
//
//	if(!is_ipaddr(network_interface_ip.c_str(), &sin_addr) )
//	{
//		EXCEPT("My IP address is invalid: %s",network_interface_ip.c_str());
//	}
//
//	has_sin_addr = true;
//	ip_addr = ntohl( sin_addr.s_addr );
//	ipaddr_initialized = TRUE;
//}

#ifdef WIN32
	// see below comment in init_hostname() to learn why we must
	// include condor_io in this module.
#include "condor_io.h"
#endif

//void
//init_hostnames()
//{
//    char *tmp, hostbuf[MAXHOSTNAMELEN];
//    hostbuf[0]='\0';
//#ifdef WIN32
//	// There are a  tools in Condor, like
//	// condor_history, which do not use any CEDAR sockets but which call
//	// some socket helper functions like gethostbyname().  These helper
//	// functions will fail unless WINSOCK is initialized.  WINSOCK
//	// is initialized via a global constructor in CEDAR, so we must
//	// make certain we call at least one CEDAR function so the linker
//	// brings in the global constructor to initialize WINSOCK!
//	// In addition, some global constructors end up calling
//	// init_hostnames(), and thus will fail if the global constructor
//	// in CEDAR is not called first.  Instead of relying upon a
//	// specified global constructor ordering (which we cannot),
//	// we explicitly invoke SockInitializer::init() right here -Todd T.
//	SockInitializer startmeup;
//	startmeup.init();
//#endif
//
//    if( hostname ) {
//        free( hostname );
//    }
//    if( full_hostname ) {
//        free( full_hostname );
//        full_hostname = NULL;
//    }
//
//	dprintf( D_HOSTNAME, "Finding local host information, "
//			 "calling gethostname()\n" );
//
//        // Get our local hostname, and strip off the domain if
//        // gethostname returns it.
//    if( condor_gethostname(hostbuf, sizeof(hostbuf)) == 0 ) {
//        if( hostbuf[0] ) {
//            if( (tmp = strchr(hostbuf, '.')) ) {
//                    // There's a '.' in the hostname, assume we've got
//                    // the full hostname here, save it, and trim the
//                    // domain off and save that as the hostname.
//                full_hostname = strdup( hostbuf );
//				dprintf( D_HOSTNAME, "gethostname() returned fully "
//						 "qualified name \"%s\"\n", hostbuf );
//                *tmp = '\0';
//            } else {
//				dprintf( D_HOSTNAME, "gethostname() returned a host "
//						 "with no domain \"%s\"\n", hostbuf );
//			}
//            hostname = strdup( hostbuf );
//        } else {
//            EXCEPT( "gethostname succeeded, but hostbuf is empty" );
//        }
//    } else {
//        EXCEPT( "gethostname failed, errno = %d",
//#ifndef WIN32
//                errno );
//#else
//                WSAGetLastError() );
//#endif
//    }
//    if( ! full_hostname ) {
//		init_full_hostname();
//	}
//	hostnames_initialized = TRUE;
//}

// Returns true if given attribute is used by Condor to advertise the
// IP address of the sender.  This is used by
// ConvertDefaultIPToSocketIP().

static bool is_sender_ip_attr(char const *attr_name)
{
    if(strcmp(attr_name,ATTR_MY_ADDRESS) == 0) return true;
    if(strcmp(attr_name,ATTR_TRANSFER_SOCKET) == 0) return true;
	size_t attr_name_len = strlen(attr_name);
    if(attr_name_len >= 6 && strcasecmp(attr_name+attr_name_len-6,"IpAddr") == 0)
	{
        return true;
    }
    return false;
}

void ConfigConvertDefaultIPToSocketIP()
{
		// do not need to call init_ipaddr() since init_ipaddr() has no effect
		// on this function.
//	if( ! ipaddr_initialized ) {
//		init_ipaddr(0);
//	}

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

static bool IPMatchesNetworkInterfaceSetting(char const *ip)
{
		// Just in case our mechanism for iterating through the interfaces
		// is not perfect, treat NETWORK_INTERFACE=* specially here so we
		// are guaranteed to return true.
	return network_interface_matches_all ||
		configured_network_interface_ips.count(ip) != 0;
}


void ConvertDefaultIPToSocketIP(char const *attr_name,char const *old_expr_string,char **new_expr_string,Stream& s)
{
	*new_expr_string = NULL;

	if( !enable_convert_default_IP_to_socket_IP ) {
		return;
	}

    if(!is_sender_ip_attr(attr_name)) {
		return;
	}

	char const *my_default_ip = my_ip_string();
	char const *my_sock_ip = s.my_ip_str();
	if(!my_default_ip || !my_sock_ip) {
		return;
	}
	if(strcmp(my_default_ip,my_sock_ip) == 0) {
		return;
	}
	condor_sockaddr sock_addr;
	if (sock_addr.from_ip_string(my_sock_ip) && sock_addr.is_loopback()) {
            // We must be talking to another daemon on the same
			// machine as us.  We don't want to replace the default IP
			// with this one, since nobody outside of this machine
			// will be able to contact us on that IP.
		return;
	}
	if( !IPMatchesNetworkInterfaceSetting(my_sock_ip) ) {
		return;
	}

	char const *ref = strstr(old_expr_string,my_default_ip);
	if(ref) {
			// the match must not be followed by any trailing digits
			// GOOD: <MMM.MMM.M.M:port?p>   (where M is a matching digit)
			// BAD:  <MMM.MMM.M.MN:port?p>  (where N is a non-matching digit)
		if( isdigit(ref[strlen(my_default_ip)]) ) {
			ref = NULL;
		}
	}
	if(ref) {
            // Replace the default IP address with the one I am actually using.

		int pos = ref-old_expr_string; // position of the reference
		int my_default_ip_len = strlen(my_default_ip);
		int my_sock_ip_len = strlen(my_sock_ip);

		*new_expr_string = (char *)malloc(strlen(old_expr_string) + my_sock_ip_len - my_default_ip_len + 1);
		ASSERT(*new_expr_string);

		strncpy(*new_expr_string, old_expr_string,pos);
		strcpy(*new_expr_string+pos, my_sock_ip);
		strcpy(*new_expr_string+pos+my_sock_ip_len, old_expr_string+pos+my_default_ip_len);

		dprintf(D_NETWORK,"Replaced default IP %s with connection IP %s "
				"in outgoing ClassAd attribute %s.\n",
				my_default_ip,my_sock_ip,attr_name);
	}
}

void ConvertDefaultIPToSocketIP(char const *attr_name,char **expr_string,Stream& s)
{
	char *new_expr_string = NULL;
	ConvertDefaultIPToSocketIP(attr_name,*expr_string,&new_expr_string,s);
	if(new_expr_string) {
		//The expression was updated.  Replace the old expression with
		//the new one.
		free(*expr_string);
		*expr_string = new_expr_string;
	}
}

void ConvertDefaultIPToSocketIP(char const *attr_name,std::string &expr_string,Stream& s)
{
	char *new_expr_string = NULL;
	ConvertDefaultIPToSocketIP(attr_name,expr_string.c_str(),&new_expr_string,s);
	if(new_expr_string) {
		//The expression was updated.  Replace the old expression with
		//the new one.
		expr_string = new_expr_string;
		free(new_expr_string);
	}
}
