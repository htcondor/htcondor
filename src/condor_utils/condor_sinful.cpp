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
Sinful::setPort(char const *port)
{
	ASSERT(port);
	m_port = port;
	regenerateStrings();
}
void
Sinful::setPort(int port)
{
	std::ostringstream tmp;
	tmp << port;
	m_port = tmp.str();
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

std::vector< condor_sockaddr > *
Sinful::getAddrs() const {
	return new std::vector< condor_sockaddr >( addrs );
}

void
Sinful::addAddrToAddrs( const condor_sockaddr & sa ) {
	addrs.push_back( sa );
	StringList sl;
	for( unsigned i = 0; i < addrs.size(); ++i ) {
		sl.append( addrs[i].to_ip_and_port_string().c_str() );
	}
	char * slString = sl.print_to_delimed_string( "+" );
	setParam( "addrs", slString );
	free( slString );
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

bool hasTwoColons( char const * sinful ) {
	const char * firstColon = strchr( sinful, ':' );
	if( firstColon && strchr( firstColon + 1, ':' ) ) {
		return true;
	}
	return false;
}

Sinful::Sinful( char const * sinful ) {
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
			// If this is a naked IPv6 address, reject, since we can't
			// reliably tell where the address ends and the port begins.
			if( hasTwoColons( sinful ) ) {
				m_valid = false;
				return;
			}

			// Otherwise, it may be an unbracketed original Sinful from
			// an old implementation of CCB.
			formatstr( m_sinfulString, "<%s>", sinful );
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
				StringList sl( addrsString, "+" );
				sl.rewind();
				char * addrString = NULL;
				while( (addrString = sl.next()) != NULL ) {
					condor_sockaddr sa;
					if( sa.from_ip_and_port_string( addrString ) ) {
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


#define PUBLIC_NETWORK_NAME "Internet"

class SourceRoute {
	public:
		SourceRoute(	condor_sockaddr sa, const std::string & networkName ) :
			p( sa.get_protocol() ), a( sa.to_ip_string() ), port( sa.get_port() ), n( networkName ) { }
		SourceRoute(	condor_protocol protocol,
						const std::string & address,
						int portNumber,
						const std::string & networkName ) :
						p( protocol ), a( address ), port( portNumber ), n( networkName ) {}
		SourceRoute( 	const SourceRoute & ma, const std::string & networkName ) :
			p( ma.p ), a( ma.a ), port( ma.port ), n( networkName ) {}

		condor_protocol getProtocol() const { return p; }
		const std::string & getAddress() const { return a; }
		int getPort() const { return port; }
		const std::string & getNetworkName() const { return n; }
		condor_sockaddr getSockAddr() const;

		// Optional attributes.
		void setSharedPortID( const std::string & spid ) { this->spid = spid; }
		void setCCBID( const std::string & ccbid ) { this->ccbid = ccbid; }
		void setCCBSharedPortID( const std::string & ccbspid ) { this->ccbspid = ccbspid; }
		void setAlias( const std::string & alias ) { this->alias = alias; }

		// Optional attributes are empty if unset.
		const std::string & getSharedPortID() const { return spid; }
		const std::string & getCCBID() const { return ccbid; }
		const std::string & setCCBSharedPortID() const { return ccbspid; }
		const std::string & getAlias() const { return alias; }

		std::string serialize();

	private:
		// Required.
		condor_protocol p;
		std::string a;
		int port;
		std::string n;

		// Optional.
		std::string spid;
		std::string ccbid;
		std::string ccbspid;
		std::string alias;
};

condor_sockaddr SourceRoute::getSockAddr() const {
	condor_sockaddr sa;
	sa.from_ip_string( a.c_str() );
	sa.set_port( port );
	if( sa.get_protocol() != p ) {
		dprintf( D_NETWORK, "Warning -- protocol of source route doesn't match its address in getSockAddr().\n" );
	}
	return sa;
}

std::string SourceRoute::serialize() {
	std::string rv;
	formatstr( rv, "p=\"%s\"; a=\"%s\"; port=%d; n=\"%s\";", condor_protocol_to_str( p ).c_str(), a.c_str(), port, n.c_str() );
	if(! alias.empty()) { rv += " alias=\"" + alias + "\";"; }
	if(! spid.empty()) { rv += " spid=\"" + spid + "\";"; }
	if(! ccbid.empty()) { rv += " ccbid=\"" + ccbid + "\";"; }
	if(! ccbspid.empty()) { rv += " ccbspid=\"" + ccbspid + "\";"; }
	formatstr( rv, "[ %s ]", rv.c_str() );
	return rv;
}

// You must delete the returned pointer (if it's not NULL).
// A simple route has only a condor_sockaddr and a network name.
SourceRoute * simpleRouteFromSinful( const Sinful & s ) {
	if(! s.valid()) { return NULL; }
	if( s.getHost() == NULL ) { return NULL; }

	condor_sockaddr primary;
	bool primaryOK = primary.from_ip_string( s.getHost() );
	if(! primaryOK) { return NULL; }

	int portNo = s.getPortNum();
	if( portNo == -1 ) { return NULL; }

	return new SourceRoute( primary.get_protocol(), primary.to_ip_string(), portNo, PUBLIC_NETWORK_NAME );
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

void Sinful::parseV1String() {
	// The correct way to do this is to faff about with ClassAds, but
	// they make it uneccessarily hard; for now, sscanf() do.

	if( m_v1String[0] != '{' ) {
		m_valid = false;
		return;
	}

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

	std::vector< SourceRoute > v;
	while( (next = strchr( remainder, '[' )) != NULL ) {
		remainder = next;
		const char * open = remainder;
		remainder = strchr( remainder, ']' );
		if( remainder == NULL ) { m_valid = false; return; }

		// Yes, yes, yes, I know.
		char nameBuffer[64];
		char addressBuffer[64];
		int port = -1;
		char protocolBuffer[16];
		int matches = sscanf( open, "[ p=%16s a=%64s port=%d; n=%64s ",
			protocolBuffer, addressBuffer, & port, nameBuffer );
		if( matches != 4 ) { m_valid = false; return; }

		if( (! stripQuotesAndSemicolon( nameBuffer )) ||
			(! stripQuotesAndSemicolon( addressBuffer )) ||
			(! stripQuotesAndSemicolon( protocolBuffer )) ) {
			m_valid = false;
			return;
		}

		condor_protocol protocol = str_to_condor_protocol( protocolBuffer );
		if( protocol <= CP_INVALID_MIN || protocol >= CP_INVALID_MAX ) {
			if( protocol == CP_PRIMARY ) {
				m_host = addressBuffer;
				formatstr( m_port, "%d", port );
			} else {
				m_valid = false;
				return;
			}
		}
		SourceRoute sr( protocol, addressBuffer, port, nameBuffer );

		// Look for alias, spid, ccbid, ccbspid.  Start by scanning
		// past the spaces we know sscanf() matched above.
		const char * parsed = open;
		for( unsigned i = 0; i < 5; ++i ) {
			parsed = strchr( parsed, ' ' );
			assert( parsed != NULL );
			++parsed;
		}

		// ... this may not work yet ...
		const char * next = NULL;
		while( (next = strchr( parsed, ' ' )) != NULL && next < remainder ) {
			const char * equals = strchr( parsed, '=' );
			if( equals == NULL ) { m_valid = false; return; }

			char attr[16];
			unsigned length = equals - (parsed + 1);
			if( length >= 16 ) { m_valid = false; return; }
			memcpy( attr, parsed + 1, length );
			attr[length] = '\0';

			dprintf( D_ALWAYS, "Found attribute '%s'.\n", attr );

			parsed = next;
			++parsed;
		}

		// Make sure the route is properly terminated.
		if( parsed[0] != ']' ) {
			m_valid = false;
			return;
		}

		if( protocol != CP_PRIMARY ) {
			v.push_back( sr );
		}
	}

	// Make sure the list is properly terminated.
	const char * closingBrace = strchr( remainder, '}' );
	if( closingBrace == NULL ) {
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
	//shared port ID, all must have one, and it must be the same.
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

	// Determine the alias, if any.  (TODO)

	// Determine the private network name, if any.
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

	// Determine the CCB contact string, if any.  (TODO)

	// Determine the set of public addresses.
	for( unsigned i = 0; i < v.size(); ++i ) {
		if(! v[i].getCCBID().empty()) { continue; }

		if( v[i].getNetworkName() == PUBLIC_NETWORK_NAME ) {
			addAddrToAddrs( v[i].getSockAddr() );
		}
	}

	// Determine the private network address, if any.
	for( unsigned i = 0; i < v.size(); ++i ) {
		if(! v[i].getCCBID().empty()) { continue; }
		if( v[i].getNetworkName() == PUBLIC_NETWORK_NAME ) { continue; }

		if( getPrivateAddr() == NULL ) {
			// setPrivateAddr( Sinful::privateAddressString( v[i].getSockAddr(), getSharedPortID() ).c_str() );
			Sinful p( v[i].getSockAddr().to_ip_and_port_string().c_str() );
			p.setSharedPortID( getSharedPortID() );
			setPrivateAddr( p.getSinful() );
		} else {
			m_valid = false;
			return;
		}
	}

	// Determine noUDP.  (TODO)


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
			condor_sockaddr sa;
			bool saOK = sa.from_ip_and_port_string( getPrivateAddr() );
			if( ! saOK ) {
				m_valid = false;
				return;
			}
			SourceRoute sr( sa, getPrivateNetworkName() );
			v.push_back( sr );
		}
	}

	// If we have a CCB address, advertise all CCB addresses.  Otherwise,
	// advertise all of our public addresses.
	if( getCCBContact() != NULL ) {
		StringList brokers( getCCBContact(), " " );
		brokers.rewind();
		char * contact = NULL;
		while( (contact = brokers.next()) != NULL ) {
			MyString ccbAddr, ccbId;
			MyString peer( "er, constructing v1 Sinful string" );
			bool contactOK = CCBClient::SplitCCBContact( contact, ccbAddr, ccbId, peer, NULL );
			if(! contactOK ) {
				m_valid = false;
				return;
			}

			Sinful s( ccbAddr.c_str() );
			SourceRoute * sr = simpleRouteFromSinful( s );
			if(sr == NULL) {
				m_valid = false;
				return;
			}

			sr->setCCBID( ccbId.c_str() );
			if( s.getSharedPortID() != NULL ) {
				sr->setCCBSharedPortID( s.getSharedPortID() );
			}
			v.push_back( * sr );
			delete sr;
		}
	} else {
		for( unsigned i = 0; i < publics.size(); ++i ) {
			v.push_back( publics[i] );
		}
	}

	// Set the host alias, if present, on all addresses.
	if( getAlias() != NULL ) {
		std::string alias( getAlias() );
		for( unsigned i = 0; i < v.size(); ++i ) {
			v[i].setAlias( alias );
		}
	}

	// Finally, set the shared port ID, if present, on all addresses.
	if( getSharedPortID() != NULL ) {
		std::string spid( getSharedPortID() );
		for( unsigned i = 0; i < v.size(); ++i ) {
			v[i].setSharedPortID( spid );
		}
	}

	// Deal with the noUDP flag.  (TODO)

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

	// A unit test will verify that we did this correctly. (TODO)
}
