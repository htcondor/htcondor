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
			ipbest = ipv4;
		} else {
			ASSERT(addr.is_ipv6());
			ipv6 = interface_pattern;
			ipbest = ipv6;
		}
		if( network_interface_ips ) {
			network_interface_ips->insert( interface_pattern );
		}

		dprintf(D_HOSTNAME,"%s=%s, so choosing IP %s\n",
				interface_param_name,
				interface_pattern,
				ipbest.c_str());
				// addr.to_ip_string().Value());

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
				interface_param_name, interface_pattern);
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
    if(strcasecmp(attr_name,ATTR_MY_ADDRESS) == 0) return true;
    if(strcasecmp(attr_name,ATTR_TRANSFER_SOCKET) == 0) return true;
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

// Only needed for these next two functions;
// #include should be deleted when ConvertDefaultIPToSocketIP is.
#include "condor_daemon_core.h"

void ConvertDefaultIPToSocketIP(char const * attr_name, std::string & expr_string, Stream & s )
{
	static bool loggedNullDCMessage = false;
	static bool loggedConfigMessage = false;

	// We can't practically do a conversion if daemonCore isn't present; this
	// happens in standard universe.  We can't move this test into
	// ConfigConvertDefaultIPToSocketIP because it gets called before
	// daemonCore is created.
	if( daemonCore == NULL ) {
		if( ! loggedNullDCMessage ) {
			dprintf( D_NETWORK | D_VERBOSE, "Address rewriting: disabled: no daemon core.\n" );
			loggedNullDCMessage = true;
		}
		return;
	}

	if( ! enable_convert_default_IP_to_socket_IP ) {
		if( ! loggedConfigMessage ) {
			dprintf( D_NETWORK | D_VERBOSE, "Address rewriting: disabled: by configuration.\n" );
			loggedConfigMessage = true;
		}
		return;
	}

	if( ! is_sender_ip_attr( attr_name ) ) {
		// Reduce log spam.  Since all of our subsequent messages include the
		// attribute name, we don't have to print a message noting that we
		// tried to rewrite it.
		// dprintf( D_NETWORK | D_VERBOSE, "Address rewriting: '%s' is not an attribute which might contain the sender's IP address.\n", attr_name );
		return;
	}

	// Skip if Stream doesn't have address associated with it
	condor_sockaddr connectionSA;
	if( ! connectionSA.from_ip_string( s.my_ip_str() ) ) {
		dprintf( D_NETWORK | D_VERBOSE, "Address rewriting: failed for attribute '%s' (%s): failed to generate socket address from stream's IP string (%s).\n", attr_name, expr_string.c_str(), s.my_ip_str() );
		return;
	}

	// Skip if it's not a string literal.
	if( * ( expr_string.rbegin() ) != '"' ) {
		dprintf( D_NETWORK | D_VERBOSE, "Address rewriting: failed for attribute '%s' (%s): failed to parse. Missing closing double quotation mark.\n", attr_name, expr_string.c_str() );
		return;
	}

	const char * delimiter = " = \"";
	size_t delimpos = expr_string.find( delimiter );
	// Skip if doesn't look like a string
	if( delimpos == std::string::npos ) {
		dprintf( D_NETWORK | D_VERBOSE, "Address rewriting: failed for attribute '%s' (%s): failed to parse. Missing assignment.\n", attr_name, expr_string.c_str() );
		return;
	}

	size_t string_start_pos = delimpos + strlen( delimiter );
	// string_end_pos is one beyond last character of String literal.
	size_t string_end_pos = expr_string.length() - 1;
	size_t string_len = string_end_pos - string_start_pos;

	// Skip if it doesn't look like a Sinful
	if( expr_string[string_start_pos] != '<' ) {
		dprintf( D_NETWORK | D_VERBOSE, "Address rewriting: failed for attribute '%s' (%s): failed to parse. Missing opening <.\n", attr_name, expr_string.c_str() );
		return;
	}
	if( expr_string[string_end_pos - 1] != '>' ) {
		dprintf( D_NETWORK | D_VERBOSE, "Address rewriting: failed for attribute '%s' (%s): failed to parse. Missing closing >.\n", attr_name, expr_string.c_str() );
		return;
	}

	std::string adSinfulString = expr_string.substr( string_start_pos, string_len);
	const char *cmd_sinful = daemonCore->InfoCommandSinfulString();
	if ( cmd_sinful == NULL ) {
		dprintf( D_NETWORK | D_VERBOSE, "Address rewriting: disabled: no command port sinful string.\n" );
		return;
	}
	std::string commandPortSinfulString = cmd_sinful;

	Sinful adSinful( adSinfulString.c_str() );
	condor_sockaddr adSA;
	adSA.from_sinful( adSinful.getSinful() );

	bool rewrite_port = true;
	if (commandPortSinfulString == adSinfulString)
	{
		dprintf( D_NETWORK | D_VERBOSE, "Address rewriting: refused for attribute %s (%s): clients now choose addresses.\n", attr_name, expr_string.c_str() );
		return;
	}
	else if (param_boolean("SHARED_PORT_ADDRESS_REWRITING", false))
	{
		//
		// Wait a minute -- isn't this only supposed to happen in the collector?
		//
		const std::vector<Sinful> &commandSinfuls = daemonCore->InfoCommandSinfulStringsMyself();
		dprintf(D_NETWORK|D_VERBOSE, "Address rewriting: considering %ld command socket sinfuls.\n", commandSinfuls.size());

		bool acceptableMatch = false;
		std::vector<Sinful>::const_iterator it;
		for (it = commandSinfuls.begin(); it!=commandSinfuls.end(); it++)
		{
			commandPortSinfulString = it->getSinful();
			const Sinful &commandPortSinful = *it;
			// We assume that any sinful on the same shared port server
			// can also be rewritten.
			if ((adSinful.getSharedPortID() != NULL) && (strcmp(commandPortSinful.getHost(), adSinful.getHost()) == 0) && (commandPortSinful.getPortNum() == adSinful.getPortNum()))
			{
				acceptableMatch = true;
				break;
			}
			dprintf( D_NETWORK | D_VERBOSE, "Address rewriting: refused for attribute %s (%s): the address isn't my default address. (Command socket considered: %s, found in ad: %s)\n", attr_name, expr_string.c_str(), commandPortSinfulString.c_str(), adSinfulString.c_str());
		}

		if (!acceptableMatch)
		{
			return;
		}
	}
	else
	{
		dprintf( D_NETWORK | D_VERBOSE, "Address rewriting: refused for attribute %s (%s): the address isn't my default address. (Default: %s, found in ad: %s)\n", attr_name, expr_string.c_str(), commandPortSinfulString.c_str(), adSinfulString.c_str());
		return;
	}

	//
	// Although it's never useful to rewrite from a non-loopback to a loop-
	// back address, if there's more than one loopback address on a machine,
	// (generally but not always because the machine supports more than one
	// protocol), it's OK to rewrite from one to the other.
	//
	// Doing this is any other situation breaks, among other things,
	// ssh-to-job.  (In a design hack, the starter sends its external
	// address to the startd over the job-update socket, as part of every
	// job update ClassAd.  This causes rewriting to happen, but as the
	// the startd explicity binds the job-update socket to the loopback
	// address -- presumambly to ensure that it always works -- we need
	// to make sure we don't rewrite ATTR_STARTER_IP_ADDR when sending
	// job updates.  *sigh*)
	//
	if( (! adSA.is_loopback()) && connectionSA.is_loopback() ) {
		dprintf( D_NETWORK | D_VERBOSE, "Address rewriting: refused for attribute '%s' (%s): outbound interface is loopback but default interface is not.\n", attr_name, expr_string.c_str() );
		return;
	}

	if( adSinful.getSharedPortID() != NULL ) {
		// We're using shared port, so "our" port is actually the
		// shared port daemon's. We shouldn't be messing with that.
		// We'll rewrite the host on the bold assumption that shared
		// port daemon and I both use the same IP addresses.
		rewrite_port = false;
	}

	MyString my_sock_ip = connectionSA.to_ip_string( true );
	adSinful.setHost( my_sock_ip.Value() );
	if( rewrite_port ) {
		// connectionSA's port is whatever we happen to be using at the moment;
		// that will be meaningless if we established the connection.  What we
		// want is the port someone could contact us on.  Go rummage for one.
		int port = daemonCore->find_interface_command_port_do_not_use( connectionSA );

		// If port is 0, there is no matching listen socket. There is nothing
		// useful we can rewrite it do, so just give up and hope the default
		// is useful to someone.
		if( port == 0 ) {
			dprintf( D_NETWORK | D_VERBOSE, "Address rewriting: failed for attribute '%s' (%s): unable to find command port for outbound interface '%s'.\n", attr_name, expr_string.c_str(), s.my_ip_str() );
			return;
		}

		adSinful.setPort( port );
	}

	if( adSinful.getSinful() == adSinfulString ) {
		dprintf( D_NETWORK | D_VERBOSE, "Address rewriting: refused for attribute '%s' (%s): socket is using same address as the default one; rewrite would do nothing.\n", attr_name, expr_string.c_str() );
		return;
	}

	std::string new_expr = expr_string.substr( 0, string_start_pos );
	new_expr.append( adSinful.getSinful() );
	new_expr.append( expr_string.substr( string_end_pos ) );

	expr_string = new_expr;

	dprintf( D_NETWORK, "Address rewriting: Replaced default IP %s with "
			"connection IP %s in outgoing ClassAd attribute %s.\n",
			adSinfulString.c_str(), adSinful.getSinful(), attr_name );
}
