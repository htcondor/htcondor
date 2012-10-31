#include "condor_common.h"
#include "MyString.h"
#include "condor_sockaddr.h"
#include "condor_netaddr.h"
#include "ipv6_hostname.h"

typedef union sockaddr_storage_ptr_u {
        const struct sockaddr     *raw;
        struct sockaddr_in  *in;
        struct sockaddr_in6 *in6;
} sockaddr_storage_ptr;

condor_sockaddr condor_sockaddr::null;

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
	sockaddr_storage_ptr sock_address;
	sock_address.raw = sa;
	if (sa->sa_family == AF_INET) {
		sockaddr_in* sin = sock_address.in;
		init(sin->sin_addr.s_addr, sin->sin_port);
	} else if (sa->sa_family == AF_INET6) {
		sockaddr_in6* sin6 = sock_address.in6;
		v6 = *sin6;
	} else {
		clear();
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

MyString condor_sockaddr::to_sinful() const
{
	MyString ret;
	char tmp[IP_STRING_BUF_SIZE];
		// if it is not ipv4 or ipv6, to_ip_string_ex will fail.
	if ( !to_ip_string_ex(tmp, IP_STRING_BUF_SIZE) )
		return ret;

	if (is_ipv4()) {
		ret.formatstr("<%s:%d>", tmp, ntohs(v4.sin_port));
	}
	else if (is_ipv6()) {
		ret.formatstr("<[%s]:%d>", tmp, ntohs(v6.sin6_port));
	}

	return ret;
}

const char* condor_sockaddr::to_sinful(char* buf, int len) const
{
	char tmp[IP_STRING_BUF_SIZE];
		// if it is not ipv4 or ipv6, to_ip_string_ex will fail.
	if ( !to_ip_string_ex(tmp, IP_STRING_BUF_SIZE) )
		return NULL;

	if (is_ipv4()) {
		snprintf(buf, len, "<%s:%d>", tmp, ntohs(v4.sin_port));
	}
	else if (is_ipv6()) {
		snprintf(buf, len, "<[%s]:%d>", tmp, ntohs(v6.sin6_port));
	}

	return buf;
}

bool condor_sockaddr::from_sinful(const MyString& sinful) {
	return from_sinful(sinful.Value());
}

// faithful reimplementation of 'string_to_sin' of internet.c
bool condor_sockaddr::from_sinful(const char* sinful)
{
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
		port_len = strspn(addr, "0123456789");
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

bool condor_sockaddr::from_ip_string(const MyString& ip_string)
{
	return from_ip_string(ip_string.Value());
}

bool condor_sockaddr::from_ip_string(const char* ip_string)
{
	if (inet_pton(AF_INET, ip_string, &v4.sin_addr) == 1) {
#ifdef HAVE_STRUCT_SOCKADDR_IN_SIN_LEN
		v4.sin_len = sizeof(sockaddr_in);
#endif
		v4.sin_family = AF_INET;
		v4.sin_port = 0;
		return true;
	} else if (inet_pton(AF_INET6, ip_string, &v6.sin6_addr) == 1) {
#ifdef HAVE_STRUCT_SOCKADDR_IN_SIN_LEN
		v6.sin6_len = sizeof(sockaddr_in);
#endif
		v6.sin6_family = AF_INET6;
		v6.sin6_port = 0;
		return true;
	}
	return false;
}

/*
const char* condor_sockaddr::to_ip_string(char* buf, int len) const
{
	if (is_addr_any())
		return get_local_condor_sockaddr().to_raw_ip_string(buf, len);
	else
		return to_raw_ip_string(buf, len);
}

MyString condor_sockaddr::to_ip_string() const
{
	if (is_addr_any())
		return get_local_condor_sockaddr().to_raw_ip_string();
	else
		return to_raw_ip_string();
}
*/

const char* condor_sockaddr::to_ip_string(char* buf, int len) const
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
		if (addr[0] == 0 && addr[1] == 0 && addr[2] == ntohl(0xffff)) {
			return inet_ntop(AF_INET, (const void*)&addr[3], buf, len);
		}

		return inet_ntop(AF_INET6, &v6.sin6_addr, buf, len);
	} else {
		snprintf(buf, len, "%x INVALID ADDRESS FAMILY", (unsigned int)v4.sin_family);
		return NULL;
	}
}

MyString condor_sockaddr::to_ip_string() const
{
	char tmp[IP_STRING_BUF_SIZE];
	MyString ret;
	if ( !to_ip_string(tmp, IP_STRING_BUF_SIZE) )
		return ret;
	ret = tmp;
	return ret;
}

MyString condor_sockaddr::to_ip_string_ex() const
{
		// no need to check is_valid()
	if ( is_addr_any() )
		return get_local_ipaddr().to_ip_string();
	else
		return to_ip_string();
}

const char* condor_sockaddr::to_ip_string_ex(char* buf, int len) const
{
		// no need to check is_valid()
	if (is_addr_any())
		return get_local_ipaddr().to_ip_string(buf, len);
	else
		return to_ip_string(buf, len);
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
		return IN6_IS_ADDR_LINKLOCAL(&v6.sin6_addr);
	}
	else {

	}
	return false;
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

void condor_sockaddr::convert_to_ipv6() {
	// only ipv4 addressn can be converted
	if (!is_ipv4())
		return;
	in6_addr addr = to_ipv6_address();
	unsigned short port = get_port();
	clear();
	set_ipv6();
	set_port(port);
	v6.sin6_addr = addr;
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
		// it begins with 169.254 -> a9 fe
		uint32_t mask = 0xa9fe0000;
		return ((uint32_t)v4.sin_addr.s_addr & mask) == mask;
	} else if (is_ipv6()) {
		// it begins with fe80
		return v6.sin6_addr.s6_addr[0] == 0xfe &&
				v6.sin6_addr.s6_addr[1] == 0x80;
	}
	return false;
}

void condor_sockaddr::set_scope_id(uint32_t scope_id) {
	if (!is_ipv6())
		return;

	v6.sin6_scope_id = scope_id;
}
