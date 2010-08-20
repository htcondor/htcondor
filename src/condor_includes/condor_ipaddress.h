
#ifndef CONDOR_IPADDRESS_H
#define CONDOR_IPADDRESS_H

class ipaddr 
{
	sockaddr_in6 storage;

	const unsigned char* get_addr() const;
	unsigned char* get_addr();
	
	unsigned int& get_addr_header();
	const unsigned int& get_v4_addr() const;
	unsigned int& get_v4_addr();
	
public:
	// accepts network byte ordered ip and port
	void init(int ip, unsigned port);
	
	ipaddr();
	
	// ip address is always network byte order but port is not.
	ipaddr(int ip, unsigned port = 0);
	ipaddr(in_addr ip, unsigned port = 0);
	ipaddr(const in6_addr& in6, unsigned port = 0);
	ipaddr(sockaddr_in* sin) ;
	ipaddr(sockaddr_in6* sin6);

	sockaddr_in to_sin() const;
	const sockaddr_in6& to_sin6() const;
	bool is_ipv4() const;

};

#endif
