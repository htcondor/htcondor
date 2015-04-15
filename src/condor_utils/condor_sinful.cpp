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
#include "condor_sinful.h"
#include "condor_debug.h"
#include "internet.h"
#include "condor_attributes.h"
#include "daemon.h"	// for global_dc_sinful()

#include <sstream>

static bool
urlDecode(char const *str,size_t max,std::string &result)
{
	size_t consumed = 0;
	while(*str) {
		size_t len = strcspn(str,"%");
		if( len + consumed > max ) {
			len = max - consumed;
		}
		result.append(str,0,len);
		str += len;
		consumed += len;
		if( consumed == max ) {
			break;
		}
		if( *str == '%' ) {
			char ch = 0;
			int i;
			str++;
			consumed++;
			for(i=0;i<2;i++,str++,consumed++) {
				ch = ch<<4;
				if( *str >= '0' && *str <= '9' ) {
					ch |= *str - '0';
				}
				else if( *str >= 'a' && *str <= 'f' ) {
					ch |= *str - 'a' + 10;
				}
				else if( *str >= 'A' && *str <= 'F' ) {
					ch |= *str - 'A' + 10;
				}
				else {
					return false;
				}
			}
			result += ch;
		}
	}
	return true;
}

static inline bool
needsUrlEncodeEscape(char ch)
{
	// The following is more conservative than it needs to be.
	// At the very least, we need to escape "&;> ,"
	if( isalnum((unsigned char)ch) || ch == '.' || ch == '_' || ch == '-' || ch == ':' || ch == '#' || ch == '[' || ch == ']' || ch == '+' ) {
		return false;
	}
	return true;
}

static void
urlEncode(char const *str,std::string &result)
{
	while(*str) {
		size_t len = 0;
		while( str[len] && !needsUrlEncodeEscape(str[len]) ) {
			len++;
		}

		result.append(str,0,len);
		str += len;
		if( *str ) {
			char code[4];
			sprintf(code,"%%%02x",*str);
			result += code;
			str++;
		}
	}
}

template <typename map_type>
static bool parseUrlEncodedParams(char const *str,map_type &params)
{
	// Parse a string in format key1=value1&key2=value2&...
	// where keys and values are url-encoded using %XX escapes.
	// For delimiting key=value pairs, either '&' or ';' may be used.
	// If the same key appears multiple times, only the last one is
	// stored in the map.

	ASSERT( str );

	while( *str ) {
		while( *str == ';' || *str == '&' ) {
			str++;
		}
		if( !*str ) {
			break;
		}

		std::pair<std::string,std::string> keyval;
		size_t len = strcspn(str,"=&;");

		if( !len ) {
			return false;
		}
		if( !urlDecode(str,len,keyval.first) ) {
			return false;
		}

		str += len;

		if( *str == '=' ) {
			str++;

			len = strcspn(str,"&;");

			if( !urlDecode(str,len,keyval.second) ) {
				return false;
			}

			str += len;
		}

		// insert_result is a pair with an iterator pointing to an
		// existing conflicting member and a bool indicating success
		std::pair<typename map_type::iterator,bool> insert_result =
			params.insert(keyval);

		if( !insert_result.second ) {
			// key already in params
			ASSERT( insert_result.first->first == keyval.first );
			insert_result.first->second = keyval.second;
		}
	}
	return true;
}

template <typename map_type>
static std::string urlEncodeParams(map_type const &params)
{
	std::string result;

	typename map_type::const_iterator it;
	for(it=params.begin(); it!=params.end(); it++) {
		if( result.size() ) {
			result += "&";
		}
		urlEncode(it->first.c_str(),result);
		if( !it->second.empty() ) {
			result += "=";
			urlEncode(it->second.c_str(),result);
		}
	}

	return result;
}

Sinful::Sinful( char const * sinful, int version ) : m_version( version ) {
	if( sinful == NULL ) {
		m_version = 0;
		m_valid = true;
		return;
	}

	if( sinful[0] == '{' ) {
		m_version = 2;
		v2_serialized = sinful;
	} else if( sinful[0] == '<' ) {
		m_version = 0;
		v0_serialized = sinful;
	} else if( sinful[0] == '[' ) {
		// We do NOT deserialize v1 addresses unless explicitly commanded.
		if( m_version != 1 ) {
			m_valid = false;
			return;
		} else {
			formatstr( v1_serialized, "<%s>", sinful );
		}
	} else if( sinful[0] == '\0' ) {
		m_version = 0;
		m_valid = true;
		return;
	}

	// If we can't tell what version the sinful string is, then for backwards
	// compatibility (with CCB), we need to try wrapping it in angle brackets.
	if( m_version == -1 ) {
		char * host = NULL;
		char * port = NULL;
		char * params = NULL;

		std::string trialSinful;
		formatstr( trialSinful, "<%s>", sinful );
		if( split_sin( trialSinful.c_str(), &host, &port, &params ) ) {
			m_version = 0;
			v0_serialized = trialSinful;
		}

		free( host ); free( port ); free( params );
	}

	// If we still don't know what it is, check to see if it's an IPv6
	// address lacking [brackets]; if so, reject it, since we can't tell if
	// the last colon is a port separator or not without them.
	if( m_version == -1 ) {
		const char * first_colon = strchr(sinful, ':');
		if( first_colon && strchr( first_colon+1, ':' ) ) {
			m_valid = false;
			return;
		}
	}

	//
	// Parse the canonicalized string.
	//

	if( m_version == 0 ) {
		char * host = NULL;
		char * port = NULL;
		char * params = NULL;

		m_valid = split_sin( v0_serialized.c_str(), &host, &port, &params );

		if( m_valid ) {
			if( host ) { m_host = host; }
			if( port ) { m_port = port; }
			if( params ) {
				if( !parseUrlEncodedParams( params,m_params ) ) {
					m_valid = false;
				}
			}
		}

		free( host ); free( port ); free( params );
	} else if( m_version == 1 ) {
		char * host = NULL;
		char * port = NULL;
		char * params = NULL;

		m_valid = split_sin( v1_serialized.c_str(), &host, &port, &params );

		if( m_valid ) {
			if( host ) {
				v1_host = host;
				if( v1_host.find( ":" ) == std::string::npos ) {
					m_host = host;
				}
			}
			if( port ) { m_port = port; }
			if( params ) {
				if( !parseUrlEncodedParams( params,m_params ) ) {
					m_valid = false;
				}
			}
		}

		free( host ); free( port ); free( params );
	} else if( m_version == 2 ) {
		// FIXME... (need to set both serialized members)
	} else {
		m_valid = false;
	}

	reserialize();
}

char const *
Sinful::getParam(char const *key) const
{
	std::map<std::string,std::string>::const_iterator it = m_params.find(key);
	if( it == m_params.end() ) {
		return NULL;
	}
	return it->second.c_str();
}

void
Sinful::setParam(char const *key,char const *value)
{
	if( !value ) {
		m_params.erase(key);
	}
	else {
		m_params[key] = value;
	}
	reserialize();
}

void
Sinful::clearParams()
{
	m_params.clear();
	reserialize();
}

int
Sinful::numParams() const
{
	return m_params.size();
}

void
Sinful::setCCBContact(char const *contact)
{
	setParam(ATTR_CCBID,contact);
}

char const *
Sinful::getCCBContact() const
{
	return getParam(ATTR_CCBID);
}

void
Sinful::setSharedPortID(char const *contact)
{
	setParam(ATTR_SOCK,contact);
}

char const *
Sinful::getSharedPortID() const
{
	return getParam(ATTR_SOCK);
}

void
Sinful::setAlias(char const *alias)
{
	setParam(ATTR_ALIAS,alias);
}

char const *
Sinful::getAlias() const
{
	return getParam(ATTR_ALIAS);
}

void
Sinful::setPrivateAddr(char const *addr)
{
	setParam("PrivAddr",addr);
}

char const *
Sinful::getPrivateAddr() const
{
	return getParam("PrivAddr");
}

void
Sinful::setPrivateNetworkName(char const *addr)
{
	setParam("PrivNet",addr);
}

char const *
Sinful::getPrivateNetworkName() const
{
	return getParam("PrivNet");
}

void
Sinful::setNoUDP(bool flag)
{
	if( !flag ) {
		setParam("noUDP",NULL);
	}
	else {
		setParam("noUDP","");
	}
}

bool
Sinful::noUDP() const
{
	return getParam("noUDP") != NULL;
}

void
Sinful::setHost(char const *host)
{
	ASSERT(host);
	m_host = host;
	reserialize();
}
void
Sinful::setPort(char const *port)
{
	ASSERT(port);
	m_port = port;
	reserialize();
}
void
Sinful::setPort(int port)
{
	std::ostringstream tmp;
	tmp << port;
	m_port = tmp.str();
	reserialize();
}

void
Sinful::reserialize()
{
	//
	// v0 serialization
	//

	v0_serialized = "<";
	if (m_host.find(':') != std::string::npos &&
		m_host.find('[') == std::string::npos) {
		v0_serialized += "[";
		v0_serialized += m_host;
		v0_serialized += "]";
	} else {
		v0_serialized += m_host;
	}

	if( !m_port.empty() ) {
		v0_serialized += ":";
		v0_serialized += m_port;
	}
	if( !m_params.empty() ) {
		v0_serialized += "?";
		v0_serialized += urlEncodeParams(m_params);
	}
	v0_serialized += ">";

	//
	// v1 serialization (FIXME)
	//

	v1_serialized = v0_serialized;
}

bool
Sinful::addressPointsToMe( Sinful const &addr ) const
{
	bool addr_matches = false;

	// Confirm that ports match. Don't even bother checking the addresses if ports don't match.
	if ( getHost() && getPort() && addr.getPort() && !strcmp(getPort(),addr.getPort()) )
	{
		// Check if host addresses match
		if( addr.getHost() && !strcmp(getHost(),addr.getHost()) )
		{
			addr_matches = true;
		}

		// We may have failed to match host addresses above, but we now need
		// to cover the case of the loopback interface (aka 127.0.0.1).  A common
		// usage pattern for this method is for "this" object to represent our daemonCore 
		// command socket.  If this is the case, and the addr passed in is a loopback
		// address, consider the addresses to match.  Note we convert to a condor_sockaddr
		// so we can use method is_loopback(), which correctly handles both IPv4 and IPv6.
		Sinful oursinful( global_dc_sinful() );
		condor_sockaddr addrsock;
		if( !addr_matches && oursinful.getHost() && !strcmp(getHost(),oursinful.getHost()) &&
			addr.hasV0HostSockAddr() && addr.getV0HostSockAddr().is_loopback() )
		{
			addr_matches = true;
		}
	}

	// The addrs and ports match, but if shared_port is in use, we need to confirm the
	// shared port id also matches.
	if (addr_matches)
	{
		char const *spid = getSharedPortID();
		char const *addr_spid = addr.getSharedPortID();
		if( (spid == NULL && addr_spid == NULL) ||			// case without shared port
			(spid && addr_spid && !strcmp(spid,addr_spid)) 	// case with shared port
		  )
		{
			return true;
		}
	}

	// Public address failed to match, but now need to do it all over again checking
	// the private address.
	if( getPrivateAddr() ) {
		Sinful private_addr( getPrivateAddr() );
		return private_addr.addressPointsToMe( addr );
	}

	return false;
}

int
Sinful::getPortNum() const
{
	if( !getPort() ) {
		return -1;
	}
	return atoi( getPort() );
}

bool
Sinful::hasV0HostSockAddr() const {
	condor_sockaddr sa;
	if( ! getHost() ) { return false; }
	return sa.from_ip_string( getHost() );
}

condor_sockaddr
Sinful::getV0HostSockAddr() const {
	condor_sockaddr sa;
	ASSERT( getHost() );
	bool ok = sa.from_ip_string( getHost() );
	ASSERT( ok );
	return sa;
}

std::string
Sinful::getV0CCBEmbedding() const {
	std::string v0 = getV0();
	ASSERT( v0[0] == '<' && v0[v0.size() - 1] == '>' );
	return v0.substr( 1, v0.size() - 2 );
}

const std::string &
Sinful::serialize() const {
	if( m_version == 0 ) { return v0_serialized; }
	else if( m_version == 1 ) { return v1_serialized; }
	EXCEPT( "Asked to serialize unknown Sinful version." );
}

std::string
Sinful::logging() const {
	return logging( "[invalid]" );
}

std::string
Sinful::logging( const std::string & ifInvalid ) const {
	if( ! valid() ) { return ifInvalid; }
	return "FIXME";
}

char const *
Sinful::getV0() const {
	if( v0_serialized.empty() ) {
		return NULL;
	} else {
		return v0_serialized.c_str();
	}
}
