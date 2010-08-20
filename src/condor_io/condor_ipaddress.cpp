#include "condor_common.h"
#include "condor_ipaddress.h"

const unsigned char* ipaddr::get_addr() const {
	// field name of in6_addr is different from platform to platform
	return (const unsigned char*)&storage.sin6_addr;
}

unsigned char* ipaddr::get_addr() {
	// field name of in6_addr is different from platform to platform
	return (unsigned char*)&storage.sin6_addr;
}

unsigned int& ipaddr::get_addr_header()
{
	return *(unsigned int*)get_addr();
}

const unsigned int& ipaddr::get_v4_addr() const
{
	return *(const unsigned int*)(get_addr()+12);
}

unsigned int& ipaddr::get_v4_addr()
{
	return *(unsigned int*)(get_addr()+12);
}

void ipaddr::init(int ip, unsigned port)
{
	memset(&storage, 0, sizeof(storage));
	storage.sin6_family = AF_INET;
	storage.sin6_port = port;
	// IPv4-mapped region
	get_addr_header() = 0xffff0000;
	get_v4_addr() = (unsigned int)ip;
	//		printf("this=%p, sin6_addr=%p\n", this, &storage.sin6_addr);
	//		printf("%p %08x %p %08x\n", &get_addr_header(), get_addr_header(), &get_v4_addr(), get_v4_addr()); 
}

ipaddr::ipaddr()
{
	memset(&storage, 0, sizeof(storage));
}

ipaddr::ipaddr(int ip, unsigned port)
{
	init(ip,htons(port));
}

ipaddr::ipaddr(in_addr ip, unsigned port)
{
	init(ip.s_addr, htons(port));
}

ipaddr::ipaddr(const in6_addr& in6, unsigned port)
{
	memset(&storage, 0, sizeof(storage));
	storage.sin6_family = AF_INET6;
	storage.sin6_port = htons(port);
	storage.sin6_addr = in6;
}

ipaddr::ipaddr(sockaddr_in* sin) 
{
	init(sin->sin_addr.s_addr, sin->sin_port);
}

ipaddr::ipaddr(sockaddr_in6* sin6)
{
	storage = *sin6;
}

sockaddr_in ipaddr::to_sin() const
{
	sockaddr_in ret;
	memset(&ret, 0, sizeof(ret));
	if (!is_ipv4()) {
		// error..
		return ret;
	}
	
	ret.sin_family = AF_INET;
	ret.sin_port = storage.sin6_port;
	ret.sin_addr.s_addr = get_v4_addr();
}

const sockaddr_in6& ipaddr::to_sin6() const
{
	//printf("to_sin6\n");
	//printf("this=%p, sin6_addr=%p\n", this, &storage.sin6_addr);
	//printf("%p %08x %p %08x\n", &get_addr_header(), get_addr_header(), &get_v4_addr(), get_v4_addr()); 
	return storage;
}

bool ipaddr::is_ipv4() const
{
	return storage.sin6_family == AF_INET;
}
