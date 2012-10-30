#include "condor_common.h"
#include "MyString.h"
#include "condor_sockfunc.h"
#include "ipv6_hostname.h"
#include "ipv6_interface.h"
#include "condor_debug.h"

typedef union sockaddr_storage_ptr_u {
        const struct sockaddr     *raw;
        struct sockaddr_in  *in;
        struct sockaddr_in6 *in6;
} sockaddr_storage_ptr;

int condor_connect(int sockfd, const condor_sockaddr& addr)
{
	if (addr.is_ipv6() && addr.is_link_local()) {
		condor_sockaddr connect_addr = addr;
		connect_addr.set_scope_id(ipv6_get_scope_id());
		return connect(sockfd, connect_addr.to_sockaddr(), connect_addr.get_socklen());
	}
	return connect(sockfd, addr.to_sockaddr(), addr.get_socklen());
}

int condor_accept(int sockfd, condor_sockaddr& addr)
{
	sockaddr_storage st;
	socklen_t len = sizeof(st);
	int ret = accept(sockfd, (sockaddr*)&st, &len);
	
	if (ret >= 0) {
		addr = condor_sockaddr((sockaddr*)&st);
	}
	return ret;

}

int condor_bind(int sockfd, const condor_sockaddr& addr)
{
	if (addr.is_ipv6() && addr.is_link_local()) {
		condor_sockaddr bind_addr = addr;
		bind_addr.set_scope_id(ipv6_get_scope_id());
		return bind(sockfd, bind_addr.to_sockaddr(), bind_addr.get_socklen());
	}
	return bind(sockfd, addr.to_sockaddr(), addr.get_socklen());
}

int condor_inet_pton(const char* src, condor_sockaddr& dest)
{
	int ret;
	const char* colon = strchr(src, ':');
	if (!colon) {
		in_addr inaddr;
		ret = inet_pton(AF_INET, src, (void*)&inaddr);
		//printf("inet_pton ipv4 path, ret=%d sin=%08x\n", ret, inaddr.s_addr);
		if (ret)
			dest = condor_sockaddr(inaddr);
	}
	else
	{
		in6_addr in6addr;
		ret = inet_pton(AF_INET6, src, (void*)&in6addr);
		if (ret)
			dest = condor_sockaddr(in6addr);
	}
	return ret;
}

//int condor_socket(int socket_type, int protocol)
//{
//#ifdef CONDOR_IPV6
//	return socket(AF_INET6, socket_type, protocol);
//#else
//	return socket(AF_INET, socket_type, protocol);
//#endif
//}

int condor_sendto(int sockfd, const void* buf, size_t len, int flags,
				  const condor_sockaddr& addr)
{
	int ret;
	if (addr.is_ipv6() && addr.is_link_local()) {
		condor_sockaddr send_addr = addr;
		send_addr.set_scope_id(ipv6_get_scope_id());
		ret = sendto(sockfd, (const char*)buf, len, flags, send_addr.to_sockaddr(),
				 send_addr.get_socklen());
	} else {
		ret = sendto(sockfd, (const char*)buf, len, flags, addr.to_sockaddr(),
					 addr.get_socklen());
	}
	return ret;
}


int condor_recvfrom(int sockfd, void* buf, size_t buf_size, int flags,
		        condor_sockaddr& addr)
{
		// we can further optimize it by passing addr into recvfrom() directly
	sockaddr_storage ss;
	int ret;
	socklen_t fromlen = sizeof(ss);
	memset(&ss, 0, sizeof(ss));

	ret = recvfrom(sockfd, (char*)buf, buf_size, flags, (sockaddr*)&ss, 
		&fromlen);
	if (ret>=0) {
		addr = condor_sockaddr( (sockaddr*)&ss );
	}
	return ret;
}

int condor_getnameinfo (const condor_sockaddr& addr,
		                char * __host, socklen_t __hostlen,
		                char * __serv, socklen_t __servlen,
		                unsigned int __flags)
{
	const sockaddr* sa = addr.to_sockaddr();
	socklen_t len = addr.get_socklen();
	int ret;

	ret = getnameinfo( sa, len, __host, __hostlen, __serv, __servlen, __flags);
	return ret;
}

int condor_getaddrinfo(const char *node,
		                const char *service,
		                const addrinfo *hints,
		                addrinfo **res)
{
	int ret;

	ret = getaddrinfo(node, service, hints, res);
	return ret;
}

hostent* condor_gethostbyaddr_ipv6(const condor_sockaddr& addr)
{
	const sockaddr* sa = addr.to_sockaddr();
	sockaddr_storage_ptr sock_address;
	sock_address.raw = sa;
	int type = sa->sa_family;
	hostent* ret;
	const char* p_addr = NULL;
	int len;

	if (type == AF_INET) {
		sockaddr_in* sin4 = sock_address.in;
		p_addr = (const char*)&sin4->sin_addr;
		len = sizeof(in_addr);
	} else if (type == AF_INET6) {
		sockaddr_in6* sin6 = sock_address.in6;
		p_addr = (const char*)&sin6->sin6_addr;
		len = sizeof(in6_addr);
	} else {
		dprintf(D_ALWAYS, "condor_gethostbyaddr_ipv6 was passed an sa_family of %d. Only AF_INET (%d) and AF_INET6 (%d) can be handled.", type, int(AF_INET), int(AF_INET6));
		sockaddr_in* sin4 = sock_address.in;
		p_addr = (const char*)&sin4->sin_addr;
		len = 0;
	}

	ret = gethostbyaddr(p_addr, len, type);
	return ret;
}

int condor_getsockname_ex(int sockfd, condor_sockaddr& addr)
{
	int ret;
	ret = condor_getsockname(sockfd, addr);
	if (ret == 0 && addr.is_addr_any()) {
		unsigned short portno = addr.get_port();
		addr = get_local_ipaddr();
		addr.set_port(portno);
	}

	return ret;
}



//const char* ipv6_addr_to_hostname(const condor_sockaddr& addr, char* buf, int len)
//{
//    struct hostent  *hp;
//    struct sockaddr_in caddr;
//
//	if ( !addr.is_valid() ) return NULL;
//
//	hp = condor_gethostbyaddr_ipv6(addr);
//	if (!hp) return NULL;
//	return hp->h_name;
//}

//int ipv6_is_ipaddr(const char* host, condor_sockaddr& addr)
//{
//	int ret = FALSE;
//	in_addr v4_addr;
//	in6_addr v6_addr;
//
//	if ( inet_pton( AF_INET, host, &v4_addr) > 0 ) {
//		addr = condor_sockaddr(v4_addr, 0);
//		ret = TRUE;
//	}
//	else if ( inet_pton( AF_INET6, host, &v6_addr ) > 0 ) {
//		addr = condor_sockaddr(v6_addr, 0);
//		ret = TRUE;
//	}
//
//	return ret;
//}
