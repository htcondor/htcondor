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
#include "get_full_hostname.h"
#include "my_hostname.h"
#include "condor_attributes.h"
#include "condor_netdb.h"

static char* hostname = NULL;
static char* full_hostname = NULL;
static unsigned int ip_addr;
static struct in_addr sin_addr;
static bool has_sin_addr = false;
static int hostnames_initialized = 0;
static int ipaddr_initialized = 0;
static void init_hostnames();

extern "C" {

// Return our hostname in a static data buffer.
char *
my_hostname()
{
	if( ! hostnames_initialized ) {
		init_hostnames();
	}
	return hostname;
}


// Return our full hostname (with domain) in a static data buffer.
char*
my_full_hostname()
{
	if( ! hostnames_initialized ) {
		init_hostnames();
	}
	return full_hostname;
}


// Return the host-ordered, unsigned int version of our hostname.
unsigned int
my_ip_addr()
{
	if( ! ipaddr_initialized ) {
		init_ipaddr(0);
	}
	return ip_addr;
}


struct in_addr*
my_sin_addr()
{
	if( ! ipaddr_initialized ) {
		init_ipaddr(0);
	}
	return &sin_addr;
}


char*
my_ip_string()
{
	if( ! ipaddr_initialized ) {
		init_ipaddr(0);
	}

		// It is too risky to return whatever inet_ntoa() returns
		// directly, because the caller may unwittingly corrupt that
		// value by calling inet_ntoa() again.

	char const *str = inet_ntoa(sin_addr);
	static char buf[IP_STRING_BUF_SIZE];

	if(!str) {
		return NULL;
	}

	ASSERT(strlen(str) < IP_STRING_BUF_SIZE);

	strcpy(buf,str);
	return buf;
}

void
init_full_hostname()
{
	char *tmp;

		// If we don't have our IP addr yet, we'll get it for free if
		// we want it.  
	if( ! ipaddr_initialized ) {
		tmp = get_full_hostname( hostname, &sin_addr );
		has_sin_addr = true;
		init_ipaddr(0);
	} else {
		tmp = get_full_hostname( hostname );
	}

	if( full_hostname ) {
		free( full_hostname );
	}
	if( tmp ) {
			// Found it, use it.
		full_hostname = strdup( tmp );
		delete [] tmp;
	} else {
			// Couldn't find it, just use what we've already got. 
		full_hostname = strdup( hostname );
	}
}

void
init_ipaddr( int config_done )
{
	char *network_interface, *tmp;
	char *host;

    if( ! hostname ) {
		init_hostnames();
	}

	dprintf( D_HOSTNAME, "Trying to initialize local IP address (%s)\n", 
		 config_done ? "after reading config" : "config file not read" );

	if( config_done ) {
		if( (network_interface = param("NETWORK_INTERFACE")) ) {
			if( is_ipaddr((const char*)network_interface, &sin_addr) ) {
					// We were given a valid IP address, which we now
					// have in ip_addr.  Just make sure it's in host
					// order now:
				has_sin_addr = true;
				ip_addr = ntohl( sin_addr.s_addr );
				ipaddr_initialized = TRUE;
				dprintf( D_HOSTNAME, "Using NETWORK_INTERFACE (%s) from "
						 "config file for local IP addr\n",
						 network_interface );
			} else {
				dprintf( D_ALWAYS, 
						 "init_ipaddr: Invalid network interface string: \"%s\"\n", 
						 network_interface );
				dprintf( D_ALWAYS, "init_ipaddr: Using default interface.\n" );
			} 
			free( network_interface );
		} else {
			dprintf( D_HOSTNAME, "NETWORK_INTERFACE not in config file, "
					 "using existing value\n" );
		}			
	}

	if( ! ipaddr_initialized ) {
		if( ! has_sin_addr ) {
			dprintf( D_HOSTNAME, "Have not found an IP yet, calling "
					 "gethostbyname()\n" );
				// Get our official host info to initialize sin_addr
			if( full_hostname ) {
				host = full_hostname;
			} else {
				host = hostname;
			}
			tmp = get_full_hostname( host, &sin_addr );
			if( ! tmp ) {
				EXCEPT( "gethostbyname(%s) failed, errno = %d", host, errno );
			}
			has_sin_addr = true;
				// We don't need the full hostname, we've already got
				// that... 
			delete [] tmp;
		} else {
			dprintf( D_HOSTNAME, "Already found IP with gethostbyname()\n" );
		}
		ip_addr = ntohl( sin_addr.s_addr );
		ipaddr_initialized = TRUE;
	}
}

} /* extern "C" */

#ifdef WIN32
	// see below comment in init_hostname() to learn why we must
	// include condor_io in this module.
#include "condor_io.h"
#endif

void
init_hostnames()
{
    char *tmp, hostbuf[MAXHOSTNAMELEN];
    hostbuf[0]='\0';
#ifdef WIN32
	// There are a  tools in Condor, like
	// condor_history, which do not use any CEDAR sockets but which call
	// some socket helper functions like gethostbyname().  These helper
	// functions will fail unless WINSOCK is initialized.  WINSOCK
	// is initialized via a global constructor in CEDAR, so we must
	// make certain we call at least one CEDAR function so the linker
	// brings in the global constructor to initialize WINSOCK! 
	// In addition, some global constructors end up calling
	// init_hostnames(), and thus will fail if the global constructor
	// in CEDAR is not called first.  Instead of relying upon a
	// specified global constructor ordering (which we cannot), 
	// we explicitly invoke SockInitializer::init() right here -Todd T.
	SockInitializer startmeup;
	startmeup.init();
#endif

    if( hostname ) {
        free( hostname );
    }
    if( full_hostname ) {
        free( full_hostname );
        full_hostname = NULL;
    }

	dprintf( D_HOSTNAME, "Finding local host information, "
			 "calling gethostname()\n" );

        // Get our local hostname, and strip off the domain if
        // gethostname returns it.
    if( condor_gethostname(hostbuf, sizeof(hostbuf)) == 0 ) {
        if( hostbuf[0] ) {
            if( (tmp = strchr(hostbuf, '.')) ) {
                    // There's a '.' in the hostname, assume we've got
                    // the full hostname here, save it, and trim the
                    // domain off and save that as the hostname.
                full_hostname = strdup( hostbuf );
				dprintf( D_HOSTNAME, "gethostname() returned fully "
						 "qualified name \"%s\"\n", hostbuf );
                *tmp = '\0';
            } else {
				dprintf( D_HOSTNAME, "gethostname() returned a host "
						 "with no domain \"%s\"\n", hostbuf );
			}
            hostname = strdup( hostbuf );
        } else {
            EXCEPT( "gethostname succeeded, but hostbuf is empty" );
        }
    } else {
        EXCEPT( "gethostname failed, errno = %d",
#ifndef WIN32
                errno );
#else
                WSAGetLastError() );
#endif
    }
    if( ! full_hostname ) {
		init_full_hostname();
	}
	hostnames_initialized = TRUE;
}

// Returns true if given attribute is used by Condor to advertise the
// IP address of the sender.  This is used by
// ConvertMyDefaultIPToMySocketIP().

static bool is_sender_ip_attr(char const *attr_name)
{
    if(strcmp(attr_name,ATTR_MY_ADDRESS) == 0) return true;
    if(strcmp(attr_name,ATTR_TRANSFER_SOCKET) == 0) return true;
	size_t attr_name_len = strlen(attr_name);
    if(attr_name_len >= 6 && stricmp(attr_name+attr_name_len-6,"IpAddr") == 0)
	{
        return true;
    }
    return false;
}


void ConvertDefaultIPToSocketIP(char const *attr_name,char const *old_expr_string,char **new_expr_string,Stream& s)
{
	*new_expr_string = NULL;

    if(!is_sender_ip_attr(attr_name)) {
		return;
	}

	/*
	  Woe is us. If GCB is enabled, we should *NOT* be re-writing IP
	  addresses in the ClassAds we send out. :(  We already go
	  through a lot of trouble to make sure they're all set how they
	  should be.  This only used to work at all because of a bug in
	  GCB + CEDAR where all outbound connections were hitting
	  GCB_bind() and so we thought my_sock_ip below was the GCB
	  broker's IP, and it all "worked".  Once we're not longer
	  pounding the GCB broker for all outbound connections, this bug
	  becomes visible.  Note that we use the optional 3rd argument to
	  param_boolean() to get it to shut up, even if NET_REMAP_ENABLE
	  isn't defined in the config file, since otherwise, this would
	  completely FILL a log file at D_CONFIG or D_ALL, since we hit
	  this for *every* potentially rewritten attribute in every ClassAd
	  that's sent.
	*/
	if (param_boolean("NET_REMAP_ENABLE", false, false)) {
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
	if(strcmp(my_sock_ip,"127.0.0.1") == 0) {
            // We assume this is the loop-back interface, so we must
			// be talking to another daemon on the same machine as us.
			// We don't want to replace the default IP with this one,
			// since nobody outside of this machine will be able to
			// contact us on that IP.
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
