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
#include "condor_config.h"

/* Split "<host:port?params>" into parts: host, port, and params. If
   the port or params are not in the string, the result is set to
   NULL.  Any of the result char** values may be NULL, in which case
   they are parsed but not set.  The caller is responsible for freeing
   all result strings.
*/
static int
split_sin( const char *addr, char **host, char **port, char **params )
{
	int len;

	if( host ) *host = NULL;
	if( port ) *port = NULL;
	if( params ) *params = NULL;

	if( !addr || *addr != '<' ) {
		return 0;
	}
	addr++;

	if (*addr == '[') {
		addr++;
		// ipv6 address
		const char* pos = strchr(addr, ']');
		if (!pos) {
			// mis-match bracket
			return 0;
		}
		if ( host ) {
			*host = (char*)malloc(pos - addr + 1);
			ASSERT( *host );
			memcpy(*host, addr, pos - addr);
			(*host)[pos - addr] = '\0';
		}
		addr = pos + 1;
	} else {
		// everything else
		len = strcspn(addr,":?>");
		if( host ) {
			*host = (char *)malloc(len+1);
			ASSERT( *host );
			memcpy(*host,addr,len);
			(*host)[len] = '\0';
		}
		addr += len;
	}

	if( *addr == ':' ) {
		addr++;
		// len = strspn(addr,"0123456789");
		// Reimplemented without strspn because strspn causes valgrind
		// errors on RHEL6.
		const char * addr_ptr = addr;
		len = 0;
		while (*addr_ptr && isdigit(*addr_ptr++)) len++;

		if( port ) {
			*port = (char *)malloc(len+1);
			memcpy(*port,addr,len);
			(*port)[len] = '\0';
		}
		addr += len;
	}

	if( *addr == '?' ) {
		addr++;
		len = strcspn(addr,">");
		if( params ) {
			*params = (char *)malloc(len+1);
			memcpy(*params,addr,len);
			(*params)[len] = '\0';
		}
		addr += len;
	}

	if( addr[0] != '>' || addr[1] != '\0' ) {
		if( host ) {
			free( *host );
			*host = NULL;
		}
		if( port ) {
			free( *port );
			*port = NULL;
		}
		if( params ) {
			free( *params );
			*params = NULL;
		}
		return 0;
	}
	return 1;
}

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
			snprintf(code,sizeof(code),"%%%02x",*str);
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
	regenerateStrings();
}

void
Sinful::clearParams()
{
	m_params.clear();
	regenerateStrings();
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
	regenerateStrings();
}
void
Sinful::setPort(char const *port, bool update_all)
{
	ASSERT(port);
	m_port = port;
	if( update_all ) {
		int portnum = atoi(port);
		for( auto& addr : addrs) {
			addr.set_port(portnum);
		}
	}
	regenerateStrings();
}
void
Sinful::setPort(int port, bool update_all)
{
	m_port = std::to_string(port);
	if( update_all ) {
		for( auto& addr : addrs ) {
			addr.set_port(port);
		}
	}
	regenerateStrings();
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

		// If the primary addresses (getHost()) don't match, check the
		// other Sinful's primary against our addrs.  This isn't generally
		// sufficient, but should solve some problems.  (Since Sinful
		// (a) doesn't require that the primary address is one of the
		// addrs and (b) doesn't require that the primary address is an
		// IP literal, the full solution is way more complicated.  Simply
		// comparing the primary and addrs addresses against each other
		// could cause problems -- e.g., ::1 is a common IPv6 address in
		// addrs.  Also, the current private-address comparison is broken
		// because it discards the private network name.)
		if(! addr_matches) {
			if( addr.getHost() ) {
				condor_sockaddr other;
				other.from_ip_string( addr.getHost() );
				if( other.is_valid() ) {
					other.set_port( addr.getPortNum() );
					for( unsigned i = 0; i < this->addrs.size(); ++i ) {
						if( other == this->addrs[i] ) {
							addr_matches = true;
							break;
						}
					}
				}
			}
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
			addr.getSinful() && addrsock.from_sinful(addr.getSinful()) && addrsock.is_loopback() )
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
		// If only one address has a shared port id, check if it matches
		// the default id (usually "collector").
		if ((spid == NULL) != (addr_spid == NULL)) {
			const char *ck_spid = (spid != NULL) ? spid : addr_spid;
			std::string default_spid;
			param(default_spid, "SHARED_PORT_DEFAULT_ID");
			if ( default_spid.empty() ) {
				default_spid = "collector";
			}
			if ( !strcmp(ck_spid, default_spid.c_str()) ) {
				return true;
			}
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

const std::vector< condor_sockaddr > &
Sinful::getAddrs() const {
	return addrs;
}

void
Sinful::addAddrToAddrs( const condor_sockaddr & sa ) {
	addrs.push_back( sa );
	std::string val;
	for (const auto& addr : addrs) {
		if (!val.empty()) { val += '+'; }
		val += addr.to_ccb_safe_string();
	}
	setParam("addrs", val.c_str());
}

void
Sinful::clearAddrs() {
	addrs.clear();
	setParam( "addrs", NULL );
}

bool
Sinful::hasAddrs() {
	return (! addrs.empty());
}

void
Sinful::regenerateStrings() {
	regenerateSinfulString();
	regenerateV1String();
}

char const *
Sinful::getSinful() const {
	if( m_sinfulString.empty() ) { return NULL; }
	return m_sinfulString.c_str();
}

char const *
Sinful::getHost() const {
	if( m_host.empty() ) { return NULL; }
	return m_host.c_str();
}

char const *
Sinful::getPort() const {
	if( m_port.empty() ) { return NULL; }
	return m_port.c_str();
}

char const *
Sinful::getV1String() const {
	if( m_v1String.empty() ) { return NULL; }
	return m_v1String.c_str();
}

bool hasTwoColonsInHost( char const * sinful ) {
	const char * firstColon = strchr( sinful, ':' );
	if( firstColon == NULL ) { return false; }
	const char * secondColon = strchr( firstColon + 1, ':' );
	if( secondColon == NULL ) { return false; }
	const char * firstQuestion = strchr( sinful, '?' );
	if( firstQuestion == NULL || secondColon < firstQuestion ) { return true; }
	return false;
}

Sinful::Sinful( char const * sinful ) : m_valid(false) {
	if( sinful == NULL ) {
		// default constructor
		m_valid = true;
		return;
	}

	// Which kind of serialization is it
	switch( sinful[0] ) {
		case '<': {
			m_sinfulString = sinful;
			parseSinfulString();
		} break;

		case '[': {
			// For now, this means an unbracketed Sinful with an IPv6 address.
			// In the future, it will mean a full nested ClassAd.  We can
			// readily the distinguish the two by scanning forward for = and :;
			// if we find = first, it's a full nested ClassAd.
			formatstr( m_sinfulString, "<%s>", sinful );
			parseSinfulString();
		} break;

		case '{': {
			m_v1String = sinful;
			parseV1String();
		} break;

		default: {
			// Otherwise, it may be an unbracketed original Sinful from
			// an old implementation of CCB... or from the command line,
			// or from a config setting.
			// If it's a naked IPv6 address, add square brackets.
			if ( hasTwoColonsInHost( sinful ) ) {
				formatstr( m_sinfulString, "<[%s]>", sinful );
			} else {
				formatstr( m_sinfulString, "<%s>", sinful );
			}
			parseSinfulString();
		} break;
	}

	if( m_valid ) {
		regenerateStrings();
	}
}

void
Sinful::regenerateSinfulString()
{
	m_sinfulString = "<";
	if (m_host.find(':') != std::string::npos &&
		m_host.find('[') == std::string::npos) {
		m_sinfulString += "[";
		m_sinfulString += m_host;
		m_sinfulString += "]";
	} else
		m_sinfulString += m_host;

	if( !m_port.empty() ) {
		m_sinfulString += ":";
		m_sinfulString += m_port;
	}
	if( !m_params.empty() ) {
		m_sinfulString += "?";
		m_sinfulString += urlEncodeParams(m_params);
	}
	m_sinfulString += ">";
}

void
Sinful::parseSinfulString() {
	char * host = NULL;
	char * port = NULL;
	char * params = NULL;

	m_valid = split_sin( m_sinfulString.c_str(), & host, & port, & params );
	if(! m_valid) { return; }

	if( host ) {
		m_host = host;
		free( host );
	}

	if( port ) {
		m_port = port;
		free( port );
	}

	if( params ) {
		if( !parseUrlEncodedParams(params,m_params) ) {
			m_valid = false;
		} else {
			char const * addrsString = getParam( "addrs" );
			if( addrsString != NULL ) {
				for (const auto& addrString : StringTokenIterator(addrsString, "+")) {
					condor_sockaddr sa;
					if( sa.from_ccb_safe_string( addrString.c_str() ) ) {
						addrs.push_back( sa );
					} else {
						m_valid = false;
					}
				}
			}
		}

		free( params );
	}
}

// You must delete the returned pointer (if it's not NULL).
// A simple route has only a condor_sockaddr and a network name.
SourceRoute * simpleRouteFromSinful( const Sinful & s, char const * n = PUBLIC_NETWORK_NAME ) {
	if(! s.valid()) { return NULL; }
	if( s.getHost() == NULL ) { return NULL; }

	condor_sockaddr primary;
	bool primaryOK = primary.from_ip_string( s.getHost() );
	if(! primaryOK) { return NULL; }

	int portNo = s.getPortNum();
	if( portNo == -1 ) { return NULL; }

	return new SourceRoute( primary.get_protocol(), primary.to_ip_string(), portNo, n );
}

bool stripQuotesAndSemicolon( char * str ) {
	unsigned length = strlen( str );
	if( str[length - 1] != ';' ) { return false; }
	if( str[length - 2] != '"' ) { return false; }
	if( str[0] != '"' ) { return false; }
	memmove( str, str + 1, length - 3 );
	str[ length - 3 ] = '\0';
	return true;
}

bool stripQuotes( std::string & str ) {
	if( str[0] != '"' ) { return false; }
	if( str[str.length() - 1] != '"' ) { return false; }
	str = str.substr( 1, str.length() - 2 );
	return true;
}

#include "ccb_server.h"

bool Sinful::getSourceRoutes( std::vector< SourceRoute > & v, std::string * hostOut, std::string * portOut ) const {
	// The correct way to do this is to faff about with ClassAds, but
	// they make it uneccessarily hard; for now, sscanf() do.

	if( m_v1String[0] != '{' ) { return false; }

	// It is readily possible to represent addresses in what looks like
	// the V1 format that we can't actually store (using the original
	// Sinful data structures).  That contradiction will be resolved in
	// a later revision, which will probably also have a more-general
	// serialization.  For now, if we can't store the addresses, we
	// mark the Sinful to be invalid.

	// This a whole wad of code, but since regenerateV0String() needs to
	// be able to (almost) all of this anyway, we might as well do it
	// here and save the space by not having parallel data structures.
	// (regenerateV0String() doesn't need to handle generating addrs.)

	// Scan forward, looking for [bracketed] source routes.  Since the
	// default constructor produces an empty list, we accept one.
	const char * next = NULL;
	const char * remainder = m_v1String.c_str();

	while( (next = strchr( remainder, '[' )) != NULL ) {
		remainder = next;
		const char * open = remainder;
		remainder = strchr( remainder, ']' );
		if( remainder == NULL ) { return false; }

		// Yes, yes, yes, I know.
		char nameBuffer[65];
		char addressBuffer[65];
		int port = -1;
		char protocolBuffer[17];
		int matches = sscanf( open, "[ p=%16s a=%64s port=%d; n=%64s ",
			protocolBuffer, addressBuffer, & port, nameBuffer );
		if( matches != 4 ) { return false; }

		if( (! stripQuotesAndSemicolon( nameBuffer )) ||
			(! stripQuotesAndSemicolon( addressBuffer )) ||
			(! stripQuotesAndSemicolon( protocolBuffer )) ) {
			return false;
		}

		condor_protocol protocol = str_to_condor_protocol( protocolBuffer );
		if( protocol <= CP_INVALID_MIN || protocol >= CP_INVALID_MAX ) {
			if( protocol != CP_PRIMARY ) {
				return false;
			}
		}
		SourceRoute sr( protocol, addressBuffer, port, nameBuffer );

		// Look for alias, spid, ccbid, ccbspid, noUDP.  Start by scanning
		// past the spaces we know sscanf() matched above.
		const char * parsed = open;
		for( unsigned i = 0; i < 5; ++i ) {
			parsed = strchr( parsed, ' ' );
			assert( parsed != NULL );
			++parsed;
		}

		const char * next = NULL;
		while( (next = strchr( parsed, ' ' )) != NULL && next < remainder ) {
			const char * equals = strchr( parsed, '=' );
			if( equals == NULL ) { return false; }

			std::string attr( parsed, equals - parsed );
			std::string value( equals + 1, (next - 1) - (equals + 1) );

			if( attr == "alias" ) {
				if( ! stripQuotes( value ) ) { return false; }
				sr.setAlias( value );
			} else if( attr == "spid" ) {
				if( ! stripQuotes( value ) ) { return false; }
				sr.setSharedPortID( value );
			} else if( attr == "ccbid" ) {
				if( ! stripQuotes( value ) ) { return false; }
				sr.setCCBID( value );
			} else if( attr == "ccbspid" ) {
				if( ! stripQuotes( value ) ) { return false; }
				sr.setCCBSharedPortID( value );
			} else if( attr == "noUDP" ) {
				// noUDP is defined to be absent if false.
				if( (!value.empty()) && value != "true" ) { return false; }
				sr.setNoUDP( true );
			} else if( attr == "brokerIndex" ) {
				unsigned index;
				if( sscanf( value.c_str(), "%d", & index ) != 1 ) {
					return false;
				}
				sr.setBrokerIndex( index );
			}

			parsed = next;
			++parsed;
		}

		// Make sure the route is properly terminated.
		if( parsed[0] != ']' ) {
			return false;
		}

		// Only set the primary address values for non-broker primaries.
		if( protocol == CP_PRIMARY && sr.getCCBID().empty() ) {
			if( hostOut ) { * hostOut = addressBuffer; }
			if( portOut ) { formatstr( * portOut, "%d", port ); }
		}

		v.push_back( sr );
	}

	// Make sure we looked at least on source route.
	if( remainder == m_v1String.c_str() ) {
		return false;
	}

	// Make sure at least one source route was valid.
	if( v.size() == 0 ) {
		return false;
	}

	// Make sure the list is properly terminated.
	const char * closingBrace = strchr( remainder, '}' );
	if( closingBrace == NULL ) {
		return false;
	}

	return true;
}

#include <algorithm>

void Sinful::parseV1String() {
	std::vector< SourceRoute > v;
	if(! getSourceRoutes( v, & m_host, & m_port ) ) {
		m_valid = false;
		return;
	}

	//
	// To convert a list of source routes back into an original Sinful's
	// data structures, do the following:
	//
	//	(1) Extract the spid from each route; they must all be the same.
	//		Set the spid.
	//  (2) Extract the alias from each route; they must all be the same.
	//		Set the alias.
	//	(3) Extract the private network name from each route; they must
	//		all be the same.  Set the private network name.
	//	(4) Check all routes for ccbid.  Each route with a ccbid goes
	//		into the ccb contact list.
	//	(5) All routes without a ccbid must be "Internet" addresses or
	//		private network addresses.  The former go into addrs (and
	//		host is set to addrs[0]).  Ignore all of the latter that
	//		have an address in addrs (because those came from CCB).  The
	//		remaining address must be the private network address.
	//

	// Determine the shared port ID, if any.  If any route has a
	// shared port ID, all must have one, and it must be the same.
	const std::string & sharedPortID = v[0].getSharedPortID();
	if(! sharedPortID.empty() ) {
		setSharedPortID( v[0].getSharedPortID().c_str() );
		for( unsigned i = 0; i < v.size(); ++i ) {
			if( v[i].getSharedPortID() != sharedPortID ) {
				m_valid = false;
				return;
			}
		}
	}

	// Determine the alias, if any.  If more than one route has an alias,
	// each alias must be the same.
	std::string alias;
	for( unsigned i = 0; i < v.size(); ++i ) {
		if(! v[i].getAlias().empty()) {
			if(! alias.empty()) {
				if( alias != v[i].getAlias() ) {
					m_valid = false;
					return;
				}
			} else {
				alias = v[i].getAlias();
			}
		}
	}
	if(! alias.empty() ) {
		setAlias( alias.c_str() );
	}

	// Determine the private network name, if any.  If more than one route
	// has a private network name, each private network name must be the same.
	std::string privateNetworkName;
	for( unsigned i = 0; i < v.size(); ++i ) {
		if( v[i].getNetworkName() != PUBLIC_NETWORK_NAME ) {
			if(! privateNetworkName.empty()) {
				if( v[i].getNetworkName() != privateNetworkName ) {
					m_valid = false;
					return;
				}
			} else {
				privateNetworkName = v[i].getNetworkName();
			}
		}
	}
	if(! privateNetworkName.empty() ) {
		setPrivateNetworkName( privateNetworkName.c_str() );
	}

	//
	// Determine the CCB contact string, if any.
	//
	// Each group of routes which shared a broker index must be converted
	// back into the single original Sinful from which it sprang; that
	// Sinful can than be added to the ccbList with its CCB ID.
	//
	std::string ccbList;

	std::map< unsigned, std::string > brokerCCBIDs;
	std::map< unsigned, std::vector< SourceRoute > > brokers;
	for( unsigned i = 0; i < v.size(); ++i ) {
		if( v[i].getCCBID().empty() ) { continue; }

		SourceRoute sr = v[i];
		sr.setSharedPortID( sr.getCCBSharedPortID() );
		sr.setCCBSharedPortID( "" );
		sr.setCCBID( "" );

		unsigned brokerIndex = sr.getBrokerIndex();
		brokers[brokerIndex].push_back( sr );
		brokerCCBIDs[brokerIndex] = v[i].getCCBID();

		dprintf( D_ALWAYS, "broker %u = %s\n", brokerIndex, sr.serialize().c_str() );
	}

	for( unsigned i = 0; i < brokers.size(); ++i ) {
		std::string brokerV1String = "{";
		brokerV1String += brokers[i][0].serialize();
		for( unsigned j = 0; j < brokers[i].size(); ++j ) {
			brokerV1String += ", ";
			brokerV1String += brokers[i][j].serialize();
		}
		brokerV1String += "}";

		Sinful s( brokerV1String.c_str() );
		std::string ccbAddress = s.getCCBAddressString();
		CCBID ccbID;
		if(! CCBServer::CCBIDFromString( ccbID, brokerCCBIDs[i].c_str() )) {
			m_valid = false;
			return;
		}
		std::string ccbContactString;
		CCBServer::CCBIDToContactString( ccbAddress.c_str(), ccbID, ccbContactString );
		if (!ccbList.empty()) { ccbList += ' '; }
		ccbList += ccbContactString;
	}

	if(! ccbList.empty() ) {
		setCCBContact( ccbList.c_str() );
	}

	// Determine the set of public addresses.
	for( unsigned i = 0; i < v.size(); ++i ) {
		if( v[i].getProtocol() == CP_PRIMARY ) { continue; }
		if(! v[i].getCCBID().empty()) { continue; }

		if( v[i].getNetworkName() == PUBLIC_NETWORK_NAME ) {
			addAddrToAddrs( v[i].getSockAddr() );
		}
	}

	// Determine the private network address, if any.
	for( unsigned i = 0; i < v.size(); ++i ) {
		if(! v[i].getCCBID().empty()) { continue; }
		if( v[i].getNetworkName() == PUBLIC_NETWORK_NAME ) { continue; }

		// A route with a public address may have a private network name
		// as a result of bypassing CCB.  Those addresses are not private
		// addresses, so ignore them.
		condor_sockaddr sa = v[i].getSockAddr();
		if( std::find( addrs.begin(), addrs.end(), sa ) != addrs.end() ) {
			continue;
		}

		// There can be only one private address.
		if( getPrivateAddr() != NULL ) {
			m_valid = false;
			return;
		}

		// setPrivateAddr( Sinful::privateAddressString( v[i].getSockAddr(), getSharedPortID() ).c_str() );
		Sinful p( v[i].getSockAddr().to_ip_and_port_string().c_str() );
		p.setSharedPortID( getSharedPortID() );
		setPrivateAddr( p.getSinful() );
	}

	// Set noUDP if any route sets it.
	for( unsigned i = 0; i < v.size(); ++i ) {
		if( v[i].getNoUDP() ) {
			setNoUDP( true );
			break;
		}
	}

	m_valid = true;
}

#include "ccb_client.h"
#include "ipv6_hostname.h"

void
Sinful::regenerateV1String() {
	if(! m_valid) {
		// The empty list.
		m_v1String = "{}";
		return;
	}

	std::vector< SourceRoute > v;
	std::vector< SourceRoute > publics;

	//
	// We need to preserve the primary address to permit round-trips from
	// original serialization to v1 serialization and back again.  If we're
	// clever, we can also use the special primary-address entry to handle
	// some troublesome backwards-compability concerns: original Sinful
	// did no input validation, and an empty original Sinful is considered
	// valid.  We should also be able to maintain the invariant that all
	// addresses are protocol literals (and therefore require no lookup).
	//
	SourceRoute sr( CP_PRIMARY, m_host, getPortNum(), PUBLIC_NETWORK_NAME );
	v.push_back( sr );

	//
	// Presently,
	// each element of the list must be of one of the following forms:
	//
	// a = primary, port = port, p = IPv4, n = "internet"
	// a = primary, port = port, p = IPv6, n = "internet"
	// a = addrs[], port = port, p = IPv4, n = "internet"
	// a = addrs[], port = port, p = IPv6, n = "internet"
	//
	// a = primary, port = port, p = IPv4, n = "private"
	// a = primary, port = port, p = IPv6, n = "private"
	// a = private, port = privport, p = IPv4, n = "private"
	// a = private, port = privport, p = IPv6, n = "private"
	//
	// a = CCB[], port = ccbport, p = IPv4, n = "internet"
	// a = CCB[], port = ccbport, p = IPv6, n = "internet"
	// a = CCB[], port = ccbport, p = IPv4, n = "internet", ccbsharedport
	// a = CCB[], port = ccbport, p = IPv6, n = "internet", ccbsharedport
	//
	// Additionally, each of the above may also include sp; if any
	// address includes sp, all must include (the same) sp.
	//

	// Start by generating our list of public addresses.
	if( numParams() == 0 ) {
		condor_sockaddr sa;
		if( sa.from_ip_string( m_host ) ) {
			SourceRoute * sr = simpleRouteFromSinful( * this );
			if( sr != NULL ) {
				publics.push_back( * sr );
				delete sr;
			}
		}
	} else if( hasAddrs() ) {
		for( unsigned i = 0; i < addrs.size(); ++i ) {
			condor_sockaddr sa = addrs[i];
			SourceRoute sr( sa, PUBLIC_NETWORK_NAME );
			publics.push_back( sr );
		}
	}

	// If we have a private network, either:
	//		* add its private network address, if it exists
	//	or
	//		* add each of its public addresses.
	// In both cases, the network name for the routes being added is the
	// private network name.
	if( getPrivateNetworkName() != NULL ) {
		if( getPrivateAddr() == NULL ) {
			for( unsigned i = 0; i < publics.size(); ++i ) {
				SourceRoute sr( publics[i], getPrivateNetworkName() );
				v.push_back( sr );
			}
		} else {
			// The private address is defined to be a simple original Sinful,
			// just and ip-and-port string surrounded by brackets.  This is
			// overkill, but it's less ugly than stripping the brackets.
			Sinful s( getPrivateAddr() );
			if(! s.valid()) {
				m_valid = false;
				return;
			}

			SourceRoute * sr = simpleRouteFromSinful( s, getPrivateNetworkName() );
			if( sr == NULL ) {
				m_valid = false;
				return;
			}
			v.push_back( * sr );
			delete sr;
		}
	}

	// If we have a CCB address, add all CCB addresses.  Otherwise, add all
	// of our public addresses.
	if( getCCBContact() != NULL ) {
		unsigned brokerIndex = 0;

		for (const auto& contact : StringTokenIterator(getCCBContact())) {
			std::string ccbAddr, ccbID;
			std::string peer( "er, constructing v1 Sinful string" );
			bool contactOK = CCBClient::SplitCCBContact( contact.c_str(), ccbAddr, ccbID, peer, NULL );
			if(! contactOK ) {
				m_valid = false;
				return;
			}

			//
			// A ccbAddr is an original Sinful without the <brackets>.  It
			// may have "PrivNet", "sock", "noUDP", and "alias" set.  What
			// we want to do is add copy ccbAddr's source routes to this
			// Sinful, adding the ccbID and setting the brokerIndex, so
			// that we know how to merge them back together when regenerating
			// this Sinful's original Sinful string.
			//
			std::string ccbSinfulString;
			formatstr( ccbSinfulString, "<%s>", ccbAddr.c_str() );
			Sinful s( ccbSinfulString.c_str() );
			if(! s.valid()) { m_valid = false; return; }
			std::vector< SourceRoute > w;
			if(! s.getSourceRoutes( w )) { m_valid = false; return; }

			for( unsigned j = 0; j < w.size(); ++j ) {
				SourceRoute sr = w[j];
				sr.setBrokerIndex( brokerIndex );
				sr.setCCBID( ccbID.c_str() );

				sr.setSharedPortID( "" );
				if( s.getSharedPortID() != NULL ) {
					sr.setCCBSharedPortID( s.getSharedPortID() );
				}

				v.push_back( sr );
			}
			++brokerIndex;
		}
	}

	// We'll never use these addresses -- the CCB address will supersede
	// them -- but we need to record them to properly recreate addrs.
	for( unsigned i = 0; i < publics.size(); ++i ) {
		v.push_back( publics[i] );
	}

	// Set the host alias, if present, on all addresses.
	if( getAlias() != NULL ) {
		std::string alias( getAlias() );
		for( unsigned i = 0; i < v.size(); ++i ) {
			v[i].setAlias( alias );
		}
	}

	// Set the shared port ID, if present, on all addresses.
	if( getSharedPortID() != NULL ) {
		std::string spid( getSharedPortID() );
		for( unsigned i = 0; i < v.size(); ++i ) {
			v[i].setSharedPortID( spid );
		}
	}

	// Set noUDP, if true, on all addresses.  (We don't have to set
	// noUDP on public non-CCB addresses, or on the private address,
	// unless WANT_UDP_COMMAND_SOCKET is false.  However, we can't
	// distinguish that case from the former two unless both CCB and
	// SP are disabled.)
	if( noUDP() ) {
		for( unsigned i = 0; i < v.size(); ++i ) {
			v[i].setNoUDP( true );
		}
	}

	//
	// Now that we've generated a list of source routes, convert it into
	// a nested ClassAd list.  The correct way to do this is to faff
	// about with ClassAds, but they make it uneccessarily hard; for now,
	// I'll just generated the appropriate string directly.
	//
	m_v1String.erase();

	m_v1String += "{";
	m_v1String += v[0].serialize();
	for( unsigned i = 1; i < v.size(); ++i ) {
		m_v1String += ", ";
		m_v1String += v[i].serialize();
	}
	m_v1String += "}";
}

std::string
Sinful::getCCBAddressString() const {
	std::string ccbAddressString( getSinful() );
	assert( ccbAddressString[0] == '<' && ccbAddressString[ccbAddressString.length() - 1] == '>' );
	ccbAddressString = ccbAddressString.substr( 1, ccbAddressString.length() - 2 );
	return ccbAddressString;
}
