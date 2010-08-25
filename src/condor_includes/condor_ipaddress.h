
#ifndef CONDOR_IPADDRESS_H
#define CONDOR_IPADDRESS_H

class ipaddr 
{
	union {
		// sockaddr_in6 and sockaddr_in structure differs from OS to OS.
		// Mac OS X has a length field while Linux usually does not have 
		// the length field.
		//
		// However,
		// sin_family and sin6_family should be located at same offset.
		// after *_family field it diverges
		sockaddr_in6 v6;
		sockaddr_in v4;
		sockaddr_storage storage;
	};
	
	/*
	const unsigned char* get_addr() const;
	unsigned char* get_addr();
	
	unsigned int& get_addr_header();
	const unsigned int& get_v4_addr() const;
	unsigned int& get_v4_addr();
	*/

	void clear();
	void init(int ip, unsigned port);
	
public:
	ipaddr();
	
	// int represented ip should be network-byte order.
	// however, port is always host-byte order.
	//
	// the reason is that in Condor source code, it does not convert
	// network-byte order to host-byte order in IP address
	// but convert so in port number.
	ipaddr(int ip, unsigned short port = 0);
	ipaddr(in_addr ip, unsigned short port = 0);
	ipaddr(in6_addr ipv6, unsigned short port = 0);
	ipaddr(sockaddr_in* sin) ;
	ipaddr(sockaddr_in6* sin6);

	sockaddr_in to_sin() const;
	sockaddr_in6 to_sin6() const;
	bool is_ipv4() const;

	// addr_any and loopback involve protocol dependent constant
	// like INADDR_ANY, IN6ADDR_ANY, ...
	//
	// so, it would be desirable to hide protocol dependent constant
	// but expose general concept like addr_any or loopback

	bool is_addr_any() const;
	void set_addr_any();
	bool is_loopback() const;
	void set_loopback();
	void set_port(unsigned short port);
	unsigned short get_port();

	// if the ipaddr contains any address (INADDR_ANY or IN6ADDR_ANY),
	// it will output as it is. 0.0.0.0 or ::0
	MyString to_string() const;

};

#endif

