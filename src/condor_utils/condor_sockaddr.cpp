#include "condor_common.h"
#include "condor_sockaddr.h"
#include "condor_netaddr.h"
#include "ipv6_hostname.h"
#include "condor_debug.h"

#include "stl_string_utils.h"

//
// We could use the parse table defaults look-up code instead, if
// we ever add enough protocols for the linear search time to matter.
//
// CP_PRIMARY is used to denote an unresolved "primary" address.  It
// exists to support round-trip conversions among Sinful serializations.
//
#define CP_PRIMARY_STRING		"primary"
#define CP_INVALID_MIN_STRING	"invalid-min"
#define CP_IPV4_STRING			"IPv4"
#define CP_IPV6_STRING			"IPv6"
#define CP_INVALID_MAX_STRING	"invalid-max"
#define CP_PARSE_INVALID_STRING	"parse-invalid"

std::string condor_protocol_to_str(condor_protocol p) {
	switch(p) {
		case CP_PRIMARY: return std::string{CP_PRIMARY_STRING};
		case CP_INVALID_MIN: return std::string{CP_INVALID_MIN_STRING};
		case CP_IPV4: return std::string{CP_IPV4_STRING};
		case CP_IPV6: return std::string{CP_IPV6_STRING};
		case CP_INVALID_MAX: return std::string{CP_INVALID_MAX_STRING};
		case CP_PARSE_INVALID: return std::string{CP_PARSE_INVALID_STRING};
		default: break;
	}
	std::string ret;
	formatstr( ret, "Unknown protocol %d\n", int(p));
	return ret;
}

condor_protocol str_to_condor_protocol( const std::string & str ) {
	if( str == CP_PRIMARY_STRING ) { return CP_PRIMARY; }
	else if( str == CP_INVALID_MIN_STRING ) { return CP_INVALID_MIN; }
	else if( str == CP_IPV4_STRING ) { return CP_IPV4; }
	else if( str == CP_IPV6_STRING ) { return CP_IPV6; }
	else if( str == CP_INVALID_MAX_STRING ) { return CP_INVALID_MAX; }
	else if( str == CP_PARSE_INVALID_STRING ) { return CP_PARSE_INVALID; }
	else { return CP_PARSE_INVALID; }
}

bool matches_withnetwork(const std::string &pattern, const char* ip_address)
{
	condor_sockaddr target;
	if (!target.from_ip_string(ip_address)) {
		return false;
	}

	condor_netaddr netaddr;
	if (!netaddr.from_net_string(pattern.c_str())) {
		return false;
	}

	return netaddr.match(target);
}

typedef union sockaddr_storage_ptr_u {
        const struct sockaddr     *raw;
        struct sockaddr_in  *in;
        struct sockaddr_in6 *in6;
} sockaddr_storage_ptr;

condor_sockaddr condor_sockaddr::null;


int condor_sockaddr::desirability() const {
	// IPv6 link local addresses are useless.  You can't use them without a
	// scope-id, and we can't determine the scope-id that someone else will
	// need.
	if(is_ipv6() && is_link_local()) { return 1; }

	if(is_loopback()) { return 2; }
	if(is_link_local()) { return 3; }
	if(is_private_network()) { return 4; }
	return 5;
}


void condor_sockaddr::clear()
{
	memset(&storage, 0, sizeof(sockaddr_storage));
}

// init only accepts network-ordered ip and port
void condor_sockaddr::init(uint32_t ip, unsigned port)
{
	clear();
#ifdef HAVE_STRUCT_SOCKADDR_IN_SIN_LEN
	v4.sin_len = sizeof(sockaddr_in);
#endif
	v4.sin_family = AF_INET;
	v4.sin_port = port;
	v4.sin_addr.s_addr = ip;
}

condor_sockaddr::condor_sockaddr()
{
	clear();
}

condor_sockaddr::condor_sockaddr(in_addr ip, unsigned short port)
{
	init(ip.s_addr, htons(port));
}

condor_sockaddr::condor_sockaddr(const in6_addr& in6, unsigned short port)
{
	clear();
#ifdef HAVE_STRUCT_SOCKADDR_IN_SIN_LEN
	v6.sin6_len = sizeof(sockaddr_in6);
#endif
	v6.sin6_family = AF_INET6;
	v6.sin6_port = htons(port);
	v6.sin6_addr = in6;
}

condor_sockaddr::condor_sockaddr(const sockaddr* sa)
{
	// Now that we actually assign a value on every path that exits the
	// constructor, this call to clear() is probably unnecessary; on the
	// other hand, it's nice to have the addresses end with a long
	// string of zeroes when looking at them in the debugger.
	clear();
	switch( sa->sa_family ) {
		case AF_INET:
			v4 = * reinterpret_cast<const sockaddr_in *>(sa);
			break;
		case AF_INET6:
			v6 = * reinterpret_cast<const sockaddr_in6 *>(sa);
			break;
		case AF_UNIX:
			storage = * reinterpret_cast<const sockaddr_storage *>(sa);
			break;
		default:
			EXCEPT( "Attempted to construct condor_sockaddr with unrecognized address family (%d), aborting.", sa->sa_family );
			break;
	}
}

condor_sockaddr::condor_sockaddr(const sockaddr_in* sin) 
{
	init(sin->sin_addr.s_addr, sin->sin_port);
}

condor_sockaddr::condor_sockaddr(const sockaddr_in6* sin6)
{
	v6 = *sin6;
}

sockaddr_in condor_sockaddr::to_sin() const
{
	return v4;
}

sockaddr_in6 condor_sockaddr::to_sin6() const
{
	return v6;
}

sockaddr_storage condor_sockaddr::to_storage() const
{
	sockaddr_storage tmp;
	if (is_ipv4())
	{
		memcpy(&tmp, &v4, sizeof(v4));
	}
	else
	{
		memcpy(&tmp, &v6, sizeof(v6));
	}
	return tmp;
}

bool condor_sockaddr::is_ipv4() const
{
	return v4.sin_family == AF_INET;
}

bool condor_sockaddr::is_ipv6() const
{
	return v6.sin6_family == AF_INET6;
}

// IN6_* macro are came from netinet/inet.h
// need to check whether it is platform-independent macro
// -- compiled on every unix/linux platforms
bool condor_sockaddr::is_addr_any() const
{
	if (is_ipv4()) {
		return v4.sin_addr.s_addr == ntohl(INADDR_ANY);
	}
	else if (is_ipv6()) {
		return IN6_IS_ADDR_UNSPECIFIED(&v6.sin6_addr);
	}
	return false;
}

void condor_sockaddr::set_addr_any()
{
	if (is_ipv4()) {
		v4.sin_addr.s_addr = ntohl(INADDR_ANY);
	}
	else if (is_ipv6()) {
		v6.sin6_addr = in6addr_any;
	}
}

bool condor_sockaddr::is_loopback() const
{
	if (is_ipv4()) {
    	return ((v4.sin_addr.s_addr & 0xFF) == 0x7F); // 127/8
	}
	else {
		return IN6_IS_ADDR_LOOPBACK( &v6.sin6_addr );
	}
}

void condor_sockaddr::set_loopback()
{
	if (is_ipv4()) {
		v4.sin_addr.s_addr = ntohl(INADDR_LOOPBACK);
	}
	else {
		v6.sin6_addr = in6addr_loopback;
	}
}

unsigned short condor_sockaddr::get_port() const
{
	if (is_ipv4()) {
		return ntohs(v4.sin_port);
	}
	else {
		return ntohs(v6.sin6_port);
	}
}

void condor_sockaddr::set_port(unsigned short port)
{
	if (is_ipv4()) {
		v4.sin_port = htons(port);
	}
	else {
		v6.sin6_port = htons(port);
	}
}

std::string condor_sockaddr::to_sinful() const
{
	// TODO: Implement in terms of Sinful object.
	std::string ret;
	char tmp[IP_STRING_BUF_SIZE];
		// if it is not ipv4 or ipv6, to_ip_string_ex will fail.
	if ( !to_ip_string_ex(tmp, IP_STRING_BUF_SIZE, true) )
		return ret;

	formatstr(ret, "<%s:%d>", tmp, ntohs(v4.sin_port));

	return ret;
}

const char* condor_sockaddr::to_sinful(char* buf, int len) const
{
	// TODO: Implement in terms of Sinful object.
	char tmp[IP_STRING_BUF_SIZE];
		// if it is not ipv4 or ipv6, to_ip_string_ex will fail.
	if ( !to_ip_string_ex(tmp, IP_STRING_BUF_SIZE, true) )
		return NULL;

	snprintf(buf, len, "<%s:%d>", tmp, ntohs(v4.sin_port));

	return buf;
}

std::string condor_sockaddr::to_sinful_wildcard_okay() const
{
	// TODO: Implement in terms of Sinful object.
	std::string ret;
	char tmp[IP_STRING_BUF_SIZE];
		// if it is not ipv4 or ipv6, to_ip_string will fail.
	if ( !to_ip_string(tmp, IP_STRING_BUF_SIZE, true) )
		return ret;

	formatstr(ret, "<%s:%d>", tmp, ntohs(v4.sin_port));

	return ret;
}

bool condor_sockaddr::from_sinful(const std::string& sinful) {
	return from_sinful(sinful.c_str());
}

// faithful reimplementation of 'string_to_sin' of internet.c
bool condor_sockaddr::from_sinful(const char* sinful)
{
	// TODO: Implement in terms of Sinful object.
	if ( !sinful ) return false;

	const char* addr = sinful;
	bool ipv6 = false;
	const char* addr_begin = NULL;
	const char* port_begin = NULL;
	int addr_len = 0;
	int port_len = 0;
	if ( *addr != '<' ) return false;
	addr++;
	if ( *addr == '[' ) {
		ipv6 = true;
		addr++;
		addr_begin = addr;

		while( *addr && *addr != ']' )
			addr++;

		if ( *addr == 0 ) return false;

		addr_len = addr-addr_begin;
		addr++;
	}
	else {
		addr_begin = addr;
		while ( *addr && *addr != ':' && *addr != '>' )
			addr++;

		if ( *addr == 0 ) return false;

		addr_len = addr-addr_begin;
		// you should not do 'addr++' here
	}

	if ( *addr == ':' ) {
		addr++;
		port_begin = addr;
		//port_len = strspn(addr, "0123456789");
		// re-implemented without strspn as strspn causes valgrind
		// errors on RHEL6.
		const char * addr_ptr = addr;
		port_len = 0;
		while (*addr_ptr && isdigit(*addr_ptr++)) port_len++;
		addr += port_len;
	}
	if ( *addr == '?' ) {
		addr++;
		int len = strcspn(addr, ">");
		addr += len;
	}

	if ( addr[0] != '>' || addr[1] != '\0' ) return false;

	clear();

	int port_no = atoi(port_begin);

	char tmp[NI_MAXHOST];
	if ( ipv6 ) {
		if (addr_len >= INET6_ADDRSTRLEN) 
			return false;
		memcpy(tmp, addr_begin, addr_len);
		tmp[addr_len] = '\0';
#ifdef HAVE_STRUCT_SOCKADDR_IN_SIN_LEN
		v6.sin6_len = sizeof(sockaddr_in6);
#endif
		v6.sin6_family = AF_INET6;
		if ( inet_pton(AF_INET6, tmp, &v6.sin6_addr) <= 0) return false;
		v6.sin6_port = htons(port_no);
	}	
	else {
		if (addr_len >= NI_MAXHOST)
			return false;
		memcpy(tmp, addr_begin, addr_len);
		tmp[addr_len] = '\0';

		if (inet_pton(AF_INET, tmp, &v4.sin_addr) > 0) {
#ifdef HAVE_STRUCT_SOCKADDR_IN_SIN_LEN
			v4.sin_len = sizeof(sockaddr_in);
#endif
			v4.sin_family = AF_INET;
			v4.sin_port = htons(port_no);
		} else {
			std::vector<condor_sockaddr> ret;
			ret = resolve_hostname(tmp);
			if (!ret.empty()) {
				*this = ret.front();
				set_port(port_no);
			} else
				return false;
		}
	}
	return true;
}

const sockaddr* condor_sockaddr::to_sockaddr() const
{
	return (const sockaddr*)&storage;
}

socklen_t condor_sockaddr::get_socklen() const
{
	if (is_ipv4())
		return sizeof(sockaddr_in);
	else if (is_ipv6())
		return sizeof(sockaddr_in6);
	else
		return sizeof(sockaddr_storage);
}

bool condor_sockaddr::from_ip_string(const std::string & ip_string)
{
	return from_ip_string(ip_string.c_str());
}

bool condor_sockaddr::from_ip_and_port_string( const char * ip_and_port_string ) {
	ASSERT( ip_and_port_string );

	char copy[IP_STRING_BUF_SIZE];
	strncpy( copy, ip_and_port_string, IP_STRING_BUF_SIZE );
	copy[IP_STRING_BUF_SIZE-1] = 0; // make sure it's null terminated.

	char * lastColon = strrchr( copy, ':' );
	if( lastColon == NULL ) { return false; }
	* lastColon = '\0';

	if( ! from_ip_string( copy ) ) { return false; }

	++lastColon;
	char * end = NULL;
	unsigned long port = strtoul( lastColon, & end, 10 );
	if( * end != '\0' ) { return false; }
	set_port( port );

	return true;
}

bool condor_sockaddr::from_ccb_safe_string( const char * ip_and_port_string ) {
	ASSERT( ip_and_port_string );

	char copy[IP_STRING_BUF_SIZE];
	strncpy( copy, ip_and_port_string, IP_STRING_BUF_SIZE );
	copy[IP_STRING_BUF_SIZE-1] = 0; // make sure it's null terminated.

	char * lastColon = strrchr( copy, '-' );
	if( lastColon == NULL ) { return false; }
	* lastColon = '\0';

	for( unsigned i = 0; i < IP_STRING_BUF_SIZE; ++i ) {
		if( copy[i] == '-' ) { copy[i] = ':'; }
	}
	if( ! from_ip_string( copy ) ) { return false; }

	++lastColon;
	char * end = NULL;
	unsigned long port = strtoul( lastColon, & end, 10 );
	if( * end != '\0' ) { return false; }
	set_port( port );

	return true;
}

bool condor_sockaddr::from_ip_string(const char* ip_string)
{
	// We're blowing an assertion on NULL input instead of 
	// just returning false because this is catching bugs, where
	// returning NULL would mask them.
	ASSERT(ip_string);

	// If we've gotten a bracketed IPv6 address, strip the brackets.
	char unbracketedString[(8 * 4) + 7 + 1];
	if( ip_string[0] == '[' ) {
		const char * closeBracket = strchr( ip_string, ']' );
		if( closeBracket != NULL ) {
			int addrLength = closeBracket - ip_string - 1;
			if( addrLength <= (8 * 4) + 7 ) {
				memcpy( unbracketedString, & ip_string[1], addrLength );
				unbracketedString[ addrLength ] = '\0';
				ip_string = unbracketedString;
			}
		}
	}

	if (inet_pton(AF_INET, ip_string, &v4.sin_addr) == 1) {
#ifdef HAVE_STRUCT_SOCKADDR_IN_SIN_LEN
		v4.sin_len = sizeof(sockaddr_in);
#endif
		v4.sin_family = AF_INET;
		v4.sin_port = 0;
		return true;
	} else if (inet_pton(AF_INET6, ip_string, &v6.sin6_addr) == 1) {
#ifdef HAVE_STRUCT_SOCKADDR_IN_SIN_LEN
		v6.sin6_len = sizeof(sockaddr_in6);
#endif
		v6.sin6_family = AF_INET6;
		v6.sin6_port = 0;
		return true;
	}
	return false;
}

const char* condor_sockaddr::to_ip_string(char* buf, int len, bool decorate) const
{
	if ( is_ipv4() ) 
		return inet_ntop(AF_INET, &v4.sin_addr, buf, len);	
	else if (is_ipv6()) {
			// [m] Special Case for IPv4-mapped-IPv6 string
			// certain implementation such as IpVerify internally uses
			// IPv6 format to store all IP addresses.
			// Although they use IPv6 address, they rely on
			// IPv4-style text representation.
			// for example, IPv4-mapped-IPv6 string will be shown as
			// a form of '::ffff:a.b.c.d', however they need
			// 'a.b.c.d'
			//
			// These reliance should be corrected at some point.
			// hopefully, at IPv6-Phase3
		const uint32_t* addr = (const uint32_t*)&v6.sin6_addr;
		char * orig_buf = buf;
		if(decorate && len > 0) {
			buf[0] = '[';
			len--;
			buf++;
		}
		const char * ret = NULL;
		if (addr[0] == 0 && addr[1] == 0 && addr[2] == ntohl(0xffff)) {
			ret = inet_ntop(AF_INET, (const void*)&addr[3], buf, len);
		} else {
			ret = inet_ntop(AF_INET6, &v6.sin6_addr, buf, len);
		}
		if(decorate && int(strlen(buf)) < (len-2)) {
			buf[strlen(buf)+1] = 0;
			buf[strlen(buf)] = ']';
		}
		if(ret) { return orig_buf; }
		return NULL;
	} else {
		snprintf(buf, len, "%x INVALID ADDRESS FAMILY", (unsigned int)v4.sin_family);
		return NULL;
	}
}

std::string condor_sockaddr::to_ip_string(bool decorate) const
{
	char tmp[IP_STRING_BUF_SIZE];
	std::string ret;
	if ( !to_ip_string(tmp, IP_STRING_BUF_SIZE, decorate) )
		return ret;
	ret = tmp;
	return ret;
}

std::string condor_sockaddr::to_ip_and_port_string() const {
	std::string ret (to_ip_string(true));
	ret += ':';
	ret += std::to_string(get_port());
	return ret;
}

std::string condor_sockaddr::to_ccb_safe_string() const {
	//
	// For backwards-compatibility with broken 8.2 Sinful code, we can't
	// allow a colon to appear in a CCB ID string.  That includes both
	// the ip:port separator, and the colons in the IPv6 literal.
	//
	char colonated[IP_STRING_BUF_SIZE];
	if(! to_ip_string( colonated, IP_STRING_BUF_SIZE, true )) {
		return std::string();
	}

	for( unsigned i = 0; colonated[i] != '\0' && i < IP_STRING_BUF_SIZE; ++i ) {
		if( colonated[i] == ':' ) { colonated[i] = '-'; }
	}

	std::string ret = colonated;
	ret += '-';
	ret += std::to_string(get_port());
	return ret;
}

std::string condor_sockaddr::to_ip_string_ex(bool decorate) const
{
		// no need to check is_valid()
	if ( is_addr_any() )
		return get_local_ipaddr(get_protocol()).to_ip_string(decorate);
	else
		return to_ip_string(decorate);
}

const char* condor_sockaddr::to_ip_string_ex(char* buf, int len, bool decorate) const
{
		// no need to check is_valid()
	if (is_addr_any())
		return get_local_ipaddr(get_protocol()).to_ip_string(buf, len, decorate);
	else
		return to_ip_string(buf, len, decorate);
}

bool condor_sockaddr::is_valid() const
{
		// the field name of sockaddr_storage differs from platform to
		// platform. For AIX, it defines __ss_family while others usually
		// define ss_family. Also, the layout is not quite same.
		// some defines length before ss_family.
		// So, here, we use sockaddr_in and sockaddr_in6 directly.
	return v4.sin_family == AF_INET || v6.sin6_family == AF_INET6;
}

bool condor_sockaddr::is_private_network() const
{
	if (is_ipv4()) {
		static bool initialized = false;
		static condor_netaddr p10;
		static condor_netaddr p172_16;
		static condor_netaddr p192_168;
		if(!initialized) {
			p10.from_net_string("10.0.0.0/8");
			p172_16.from_net_string("172.16.0.0/12");
			p192_168.from_net_string("192.168.0.0/16");
			initialized = true;
		}

		return p10.match(*this) || p172_16.match(*this) || p192_168.match(*this);
	}
	else if (is_ipv6()) {
		static bool initialized = false;
		static condor_netaddr pfc00;
		if(!initialized) {
			pfc00.from_net_string("fc00::/7");
			initialized = true;
		}

		return pfc00.match(*this);
	}
	else {

	}
	return false;
}

void condor_sockaddr::set_protocol(condor_protocol proto) {
	switch(proto) {
		case CP_IPV4: set_ipv4(); break;
		case CP_IPV6: set_ipv6(); break;
		default: ASSERT(0); break;
	}
}

condor_protocol condor_sockaddr::get_protocol() const {
	if(is_ipv4()) { return CP_IPV4; }
	if(is_ipv6()) { return CP_IPV6; }
	return CP_INVALID_MIN;
}

void condor_sockaddr::set_ipv4() {
	v4.sin_family = AF_INET;
}

void condor_sockaddr::set_ipv6() {
	v6.sin6_family = AF_INET6;
}

int condor_sockaddr::get_aftype() const {
	if (is_ipv4())
		return AF_INET;
	else if (is_ipv6())
		return AF_INET6;
	return AF_UNSPEC;
}

const uint32_t* condor_sockaddr::get_address() const {
	switch(v4.sin_family) {
	case AF_INET:
		return (const uint32_t*)&v4.sin_addr;
	case AF_INET6:
		return (const uint32_t*)&v6.sin6_addr;
	}
	return 0;
}

int condor_sockaddr::get_address_len() const {
	switch(v4.sin_family) {
	case AF_INET:
		return 1;
	case AF_INET6:
		return 4;
	}
	return 0;
}

in6_addr condor_sockaddr::to_ipv6_address() const
{
	if (is_ipv6()) return v6.sin6_addr;
	in6_addr ret;
	memset(&ret, 0, sizeof(ret));
		// the field name of struct in6_addr is differ from platform to
		// platform. thus, we use a pointer.
	uint32_t* addr = (uint32_t*)&ret;
	addr[0] = 0;
	addr[1] = 0;
	addr[2] = htonl(0xffff);
	addr[3] = v4.sin_addr.s_addr;
	return ret;
}

bool condor_sockaddr::compare_address(const condor_sockaddr& addr) const
{
	if (is_ipv4()) {
		if (!addr.is_ipv4())
			return false;
		return v4.sin_addr.s_addr == addr.v4.sin_addr.s_addr;
	} else if (is_ipv6()) {
		if (!addr.is_ipv6())
			return false;
		return memcmp((const void*)&v6.sin6_addr,
					  (const void*)&addr.v6.sin6_addr,
					  sizeof(in6_addr)) == 0;
	}
	return false;
}

// lexicographical ordering of IP address
// 1. compare aftype
// 2. compare address
// 3. compare port

bool condor_sockaddr::operator<(const condor_sockaddr& rhs) const
{
	const void* l = (const void*)&storage;
	const void* r = (const void*)&rhs.storage;
	return memcmp(l, r, sizeof(sockaddr_storage)) < 0;
}

bool condor_sockaddr::operator==(const condor_sockaddr& rhs) const
{
	const void* l = (const void*)&storage;
	const void* r = (const void*)&rhs.storage;
	return memcmp(l, r, sizeof(sockaddr_storage)) == 0;
}

bool condor_sockaddr::is_link_local() const {
	if (is_ipv4()) {
		static bool initialized = false;
		static condor_netaddr p169_254;
		if(!initialized) {
			p169_254.from_net_string("169.254.0.0/16");
			initialized = true;
		}

		return p169_254.match(*this);
	} else if (is_ipv6()) {
		// fe80::/10
		return IN6_IS_ADDR_LINKLOCAL(&v6.sin6_addr);
	}
	return false;
}

void condor_sockaddr::set_scope_id(uint32_t scope_id) {
	if (!is_ipv6())
		return;

	v6.sin6_scope_id = scope_id;
}
