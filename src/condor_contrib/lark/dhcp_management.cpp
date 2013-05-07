
#include "condor_common.h"

#include <errno.h>
#include <stdlib.h>
#include <sys/socket.h>

#include "condor_debug.h"
#include <classad/classad.h>
#include "network_adapter.unix.h"

#include "lark_attributes.h"

#include "dhcp_management.h"

#define INET_LEN 4

static char dhcp_send_header[] = {'\x01', '\x01', '\x06', '\x00'};
static char dhcp_recv_header[] = {'\x02', '\x01', '\x06', '\x00'};
static char magic_cookie[] = {'\x63', '\x82', '\x53', '\x63'};
#define DHCPDISCOVER '\x01'
#define DHCPOFFER    '\x02'
#define DHCPREQUEST  '\x03'
#define DHCPDECLINE  '\x04'
#define DHCPACK      '\x05'
#define DHCPNAK      '\x06'
#define DHCPRELEASE  '\x07'

class DHCPPacket {

public:
	DHCPPacket(uint32_t txid, const char mac_addr[IFHWADDRLEN]) : m_secs(0), m_flags(0),
		m_txid(txid), m_length(-1)
		{
		m_ciaddr.s_addr = INADDR_ANY;
		m_yiaddr.s_addr = INADDR_ANY;
		m_siaddr.s_addr = INADDR_ANY;
		m_giaddr.s_addr = INADDR_ANY;
		memcpy(m_mac_addr, mac_addr, IFHWADDRLEN);
		memcpy(m_header, dhcp_send_header, 4);
		memcpy(m_magic, magic_cookie, 4);
		bzero(m_padding_202, 202);
		}

	int makeIOV(struct iovec *&iov, size_t &iov_len, bool for_recv=false)
	{
		// See Figure 1 of RFC2131 for packet layout
		iov_len = 12;
		iov = m_iov;
		m_iov[0].iov_base = &m_header;
		m_iov[0].iov_len = 4;
		m_iov[1].iov_base = &m_txid;
		m_iov[1].iov_len = 4;
		m_iov[2].iov_base = &m_secs;
		m_iov[2].iov_len = 2;
		m_iov[3].iov_base = &m_flags;
		m_iov[3].iov_len = 2;
		m_iov[4].iov_base = &m_ciaddr;
		m_iov[4].iov_len = 4;
		m_iov[5].iov_base = &m_yiaddr;
		m_iov[5].iov_len = 4;
		m_iov[6].iov_base = &m_siaddr;
		m_iov[6].iov_len = 4;
		m_iov[7].iov_base = &m_giaddr;
		m_iov[7].iov_len = 4;
		m_iov[8].iov_base = &m_mac_addr;
		m_iov[8].iov_len = 6;
		m_iov[9].iov_base = &m_padding_202;
		m_iov[9].iov_len = 202;
		m_iov[10].iov_base = &m_magic;
		m_iov[10].iov_len = 4;
		m_iov[11].iov_base = &m_options;
		if (for_recv)
		{
			m_iov[11].iov_len = 1024;
		}
		else
		{
			if (getOptionLength() < -1)
			{
				computeLength();
			}
			m_iov[11].iov_len = getOptionLength();
		}
		return 0;
	}

	char * iterateOpts(char * start, char &option, size_t &length, char *&value)
	{
		if (start == NULL) {
			start = m_options;
		}
		option = *start;
		if (option == '\xff') {
			length = 0;
			value = start+1;
			return NULL;
		}
		if ((start - m_options >= 1024) || (m_length >= 0 && (start - m_options >= getOptionLength()))) return NULL;
		length = *(start+1);
		value = start+2;
		
		return start + length + 2;
	}

	char * setOption(char * start, char option, unsigned char length, const char *value)
	{
		if (start == NULL) {
			m_length = 236;
			start = m_options;
		}
		if (start - m_options + 1 + length >= 1024) return NULL;
		*start = option;
		if (length) {
			*(start+1) = length;
			memcpy(start+2, value, length);
			m_length += 2 + length;
		} else {
			m_length += 1;
		}
		return start+2+length;
	}

	ssize_t computeLength(ssize_t max_length = -1)
	{
		max_length -= 236;
		char * iter = NULL;
		char option;
		size_t length;
		char * value;
		while ((iter = iterateOpts(iter, option, length, value)) != NULL)
		{
			if (iter-m_options > max_length) {
				m_length = max_length + 236;
				return max_length;
			}
			if (option == '\xff') {
				m_length = value - m_options;
				return m_length;
			}
		}
		return -1;
	}

	ssize_t getLength() {return m_length;}
	ssize_t getOptionLength() const {return m_length >= 0 ? m_length - 236 : -1;}

	short m_secs;
	short m_flags;
	struct in_addr m_ciaddr;
	struct in_addr m_yiaddr;
	struct in_addr m_siaddr;
	struct in_addr m_giaddr;

	char m_mac_addr[IFHWADDRLEN];

	char m_options[1024];
	char m_header[4];
	uint32_t m_txid;
	ssize_t m_length;
	char m_magic[4];
private:
	char m_padding_202[202];
	struct iovec m_iov[12];

};

static int
get_mac_address(classad::ClassAd &machine_ad, char result[IFHWADDRLEN])
{
	std::string interface_name;
	if (!machine_ad.EvaluateAttrString(ATTR_INTERNAL_INTERFACE, interface_name)) {
		dprintf(D_ALWAYS, "Required ClassAd attribute " ATTR_INTERNAL_INTERFACE " is missing.\n");
		return 1;
	}
	if (interface_name.length() > IFNAMSIZ) {
		dprintf(D_ALWAYS, "External interface name (%s) is too long.\n", interface_name.c_str());
		return 1;
	}
	std::string cached_mac_address;
	if (machine_ad.EvaluateAttrString(ATTR_DHCP_MAC, cached_mac_address) && cached_mac_address.length() >= IFHWADDRLEN) {
		unsigned char mac_bin[IFHWADDRLEN];
		if (MacAddressHexToBin(cached_mac_address.c_str(), mac_bin))
		{
			memcpy(result, mac_bin, IFHWADDRLEN);
			return 0;
		}
	}

	int fd;
	struct ifreq iface_req;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd == -1) {
		dprintf(D_ALWAYS, "Failed to create socket for retrieving hardware address (errno=%d, %s).\n", errno, strerror(errno));
		return errno;
	}

	memset(&iface_req, '\0', sizeof(iface_req));
	strncpy(iface_req.ifr_name, interface_name.c_str(), IFNAMSIZ);
	if (-1 == ioctl(fd, SIOCGIFHWADDR, &iface_req)) {
		dprintf(D_ALWAYS, "Error when retrieving hardware address (errno=%d, %s).\n", errno, strerror(errno));
		close(fd);
		return errno;
	}
	close(fd);

	memcpy(result, iface_req.ifr_hwaddr.sa_data, IFHWADDRLEN);

	unsigned char mac_address[IFHWADDRLEN+1];
	mac_address[IFHWADDRLEN] = '\0';
	memcpy(mac_address, iface_req.ifr_hwaddr.sa_data, IFHWADDRLEN);
	char mac_address_hex[18]; mac_address_hex[17] = '\0';
	MacAddressBinToHex(mac_address, mac_address_hex);
	machine_ad.InsertAttr(ATTR_DHCP_MAC, mac_address_hex);

	return 0;
}

static void
populate_classad(DHCPPacket &packet, classad::ClassAd &machine_ad)
{
		char ip_address[INET_ADDRSTRLEN+1];
		if (inet_ntop(AF_INET, &packet.m_yiaddr, ip_address, INET_ADDRSTRLEN)) {
			machine_ad.InsertAttr(ATTR_INTERNAL_ADDRESS_IPV4, ip_address);
		}
		if (inet_ntop(AF_INET, &packet.m_giaddr, ip_address, INET_ADDRSTRLEN)) {
			machine_ad.InsertAttr(ATTR_DHCP_GATEWAY, ip_address);
		}
		char *iter = NULL, *value, option;
		size_t length;
		time_t now = 0;
		while ((iter = packet.iterateOpts(iter, option, length, value))) {
		switch (option) {
			case '\x36': // DHCP server
				char dhcp_server[INET_ADDRSTRLEN+1];
				if (length >= INET_LEN && inet_ntop(AF_INET, value, dhcp_server, INET_ADDRSTRLEN+1)) {
					machine_ad.InsertAttr(ATTR_DHCP_SERVER, dhcp_server);
				}
				break;
			case '\x01': // Subnet mask
				char subnet_mask[INET_ADDRSTRLEN+1];
				if (length >= INET_LEN && inet_ntop(AF_INET, value, subnet_mask, INET_ADDRSTRLEN+1)) {
					machine_ad.InsertAttr(ATTR_SUBNET_MASK, subnet_mask);
				}
				break;
			case '\x03': // Router/Gateway
				char gateway[INET_ADDRSTRLEN+1];
				if (length >= INET_LEN && inet_ntop(AF_INET, value, gateway, INET_ADDRSTRLEN+1)) {
					machine_ad.InsertAttr(ATTR_GATEWAY, gateway);
				}
				break;
			case '\x33': // Lease
				now = time(NULL);
				if (length >= 4) {
					uint32_t lease_time = ntohl(*(uint32_t *)value);
					machine_ad.InsertAttr(ATTR_DHCP_LEASE, (long int)lease_time, classad::Value::NO_FACTOR);
					machine_ad.InsertAttr(ATTR_DHCP_LEASE_START, (long int)now, classad::Value::NO_FACTOR);
				}
				break;
			case '\x12': // Hostname
				if (length >= 1) {
					char * hostname = new char[length+1];
					if (hostname) {
						hostname[length] = '\0';
						memcpy(hostname, value, length);
						machine_ad.InsertAttr(ATTR_LARK_HOSTNAME, hostname);
					}
					delete [] hostname;
				}
				break;
			default:
				break;
		}
		}
}

static int
send_dhcp_discovery(int fd, uint32_t txid, const char mac_address[IFHWADDRLEN])
{
	DHCPPacket packet(txid, mac_address);

	static const char packet_type = DHCPDISCOVER;
	char * iter = packet.setOption(NULL, '\x35', 1, &packet_type);
	iter = packet.setOption(iter, '\x3d', 6, mac_address);
	static const char dhcp_request[] = {'\x03', '\x01', '\x0c', '\x0f'};
	iter = packet.setOption(iter, '\x37', 4, dhcp_request);
	iter = packet.setOption(iter, '\x3c', 4, "lark");
	iter = packet.setOption(iter, '\xff', 0, NULL);
	packet.m_flags = htons(32768);

	struct iovec *iov;
	size_t iov_len;
	packet.makeIOV(iov, iov_len);

	struct msghdr msg;
	bzero(&msg, sizeof(msg));
	struct sockaddr_in dest; bzero(&dest, sizeof(dest));
	dest.sin_family = AF_INET;
	dest.sin_addr.s_addr = INADDR_BROADCAST;
	dest.sin_port = htons(67);
	msg.msg_name = &dest;
	msg.msg_namelen = sizeof(dest);
	msg.msg_iov = iov;
	msg.msg_iovlen = iov_len;
	while (-1 == sendmsg(fd, &msg, 0)) {
		if (errno == EINTR) continue;
		dprintf(D_ALWAYS, "Failed to send DHCP discover packet (errno=%d, %s)\n", errno, strerror(errno));
		return errno;
	}
	return 0;
}

static int
recv_dhcp_offer(int fd, uint32_t txid, char mac_addr[IFHWADDRLEN], unsigned secs, classad::ClassAd &machine_ad)
{
	DHCPPacket packet(txid, mac_addr);

	struct msghdr msg; memset(&msg, 0, sizeof(msg));
	ssize_t packet_size;
	time_t starttime = time(NULL);
	while (time(NULL) - starttime <= secs) {
		struct iovec *iov;
		size_t iov_len;
		packet.makeIOV(iov, iov_len, true);
		msg.msg_iov = iov;
		msg.msg_iovlen = iov_len;
		packet_size = recvmsg(fd, &msg, 0);
		if (packet_size == -1) {
			if (errno == EINTR) continue;
			if (errno == EAGAIN) {
				sleep(1);
				continue;
			}
			dprintf(D_ALWAYS, "Error when waiting for DHCP offer (errno=%d, %s).\n", errno, strerror(errno));
		}
		if (packet_size <= 236) {
			// packet is too small to be valid.
			continue;
		}
		if (memcmp(dhcp_recv_header, packet.m_header, 4) != 0) {
			dprintf(D_FULLDEBUG, "Invalid DHCP offer header.\n");
			continue;
		}
		if (packet.m_txid != txid) {
			dprintf(D_FULLDEBUG, "Invalid offer TXID: %d\n", packet.m_txid);
			continue;
		}
		if (memcmp(packet.m_mac_addr, mac_addr, IFHWADDRLEN) != 0) {
			dprintf(D_FULLDEBUG, "Invalid offer mac address.\n");
			continue;
		}
		if (memcmp(packet.m_magic, magic_cookie, 4) != 0) {
			dprintf(D_FULLDEBUG, "Invalid magic packet.\n");
			continue;
		}
		// At this point, we now believe this is a valid offer.  Populate the corresponding request packet.
		packet.computeLength(packet_size);
		char * iter = NULL;
		char option;
		size_t length;
		char * value;
		bool valid_packet = true;
		while ((iter = packet.iterateOpts(iter, option, length, value))) {
			// Note that we ignore most of the info in the offer.
			// We delay utilizing any information until the ACK.
			if (option == '\x35' && *value != DHCPOFFER) {
				valid_packet = false;
				break;
			}
		}
		if (valid_packet != true) {
			continue;
		}
		// Populate the machine ad from this packet.
		populate_classad(packet, machine_ad);
		return 0;
	}
	return 1;
}

static int
send_dhcp_request(int fd, uint32_t txid, char mac_addr[IFHWADDRLEN], const classad::ClassAd &machine_ad)
{
	std::string internal_addr;
	if (!machine_ad.EvaluateAttrString(ATTR_INTERNAL_ADDRESS_IPV4, internal_addr)) {
		dprintf(D_ALWAYS, "Missing IPv4 address on internal interface in ClassAd.\n");
		return 1;
	}
	struct in_addr ciaddr;
	if (inet_pton(AF_INET, internal_addr.c_str(), &ciaddr) != 1) {
		dprintf(D_ALWAYS, "Failed to convert %s to IPv4 address.\n", internal_addr.c_str());
		return 1;
	}
        std::string server_addr;
        if (!machine_ad.EvaluateAttrString(ATTR_DHCP_SERVER, server_addr)) {
                dprintf(D_ALWAYS, "Missing DHCP server address in ClassAd.\n");
                return 1;
        }
        struct in_addr siaddr;
        if (inet_pton(AF_INET, server_addr.c_str(), &siaddr) != 1) {
                dprintf(D_ALWAYS, "Failed to convert %s to IPv4 address.\n", server_addr.c_str());
                return 1;
        }
        std::string gateway_addr;
        if (!machine_ad.EvaluateAttrString(ATTR_DHCP_GATEWAY, gateway_addr)) {
                dprintf(D_ALWAYS, "Missing DHCP relay address in ClassAd.\n");
                return 1;
        }
        struct in_addr giaddr;
        if (inet_pton(AF_INET, gateway_addr.c_str(), &giaddr) != 1) {
                dprintf(D_ALWAYS, "Failed to convert %s to IPv4 address.\n", gateway_addr.c_str());
                return 1;
        }
	DHCPPacket request_packet(txid, mac_addr);
	request_packet.m_ciaddr.s_addr = INADDR_ANY;
	request_packet.m_yiaddr.s_addr = INADDR_ANY;
	request_packet.m_siaddr.s_addr = siaddr.s_addr;
	request_packet.m_giaddr.s_addr = giaddr.s_addr;
	char packet_type = DHCPREQUEST;
	char * req_iter = request_packet.setOption(NULL, '\x35', 1, &packet_type);
	req_iter = request_packet.setOption(req_iter, '\x3d', IFHWADDRLEN, request_packet.m_mac_addr);
	req_iter = request_packet.setOption(req_iter, '\x32', 4, (char*)&ciaddr.s_addr);
	req_iter = request_packet.setOption(req_iter, '\x36', 4, (char*)&request_packet.m_siaddr.s_addr);
	req_iter = request_packet.setOption(req_iter, '\x3c', 4, "lark");

	struct iovec *iov;
	size_t iov_len;
	request_packet.makeIOV(iov, iov_len);

	struct msghdr msg;
	bzero(&msg, sizeof(msg));
	struct sockaddr_in dest;
	dest.sin_family = AF_INET;
	dest.sin_addr.s_addr = INADDR_BROADCAST;
	dest.sin_port = htons(67);
	msg.msg_name = &dest;
	msg.msg_namelen = sizeof(dest);
	msg.msg_iov = iov;
	msg.msg_iovlen = iov_len;
	if (-1 == sendmsg(fd, &msg, 0)) {
		dprintf(D_ALWAYS, "Failed to send DHCP request packet (errno=%d, %s)\n", errno, strerror(errno));
		return errno;
	}
	return 0;
}

static int
recv_dhcp_ack(int fd, uint32_t txid, const char mac_address[IFHWADDRLEN], unsigned secs, classad::ClassAd &machine_ad)
{
	DHCPPacket packet(txid, mac_address);

	struct msghdr msg;
	ssize_t packet_size;
	time_t starttime = time(NULL);
	while (time(NULL) - starttime <= secs) {
		struct iovec *iov;
		size_t iov_len;
		packet.makeIOV(iov, iov_len, true);
		bzero(&msg, sizeof(msg));
		msg.msg_iov = iov;
		msg.msg_iovlen = iov_len;
		// TODO: make sure the returned msg_name is from the correct server.
		packet_size = recvmsg(fd, &msg, 0);
		if (packet_size == -1) {
			if (errno == EINTR) continue;
			if (errno == EAGAIN) {
				sleep(1);
				continue;
			}
			dprintf(D_ALWAYS, "Error when waiting for DHCP ACK (errno=%d, %s).\n", errno, strerror(errno));
			break;
		}
		if (packet_size <= 236) {
			// packet is too small to be valid.
			continue;
		}
		if (memcmp(dhcp_recv_header, packet.m_header, 4) != 0) {
			dprintf(D_FULLDEBUG, "Invalid DHCP offer header.\n");
			continue;
		}
		if (packet.m_txid != txid) {
			dprintf(D_FULLDEBUG, "Invalid offer TXID: %d\n", packet.m_txid);
			continue;
		}
		if (memcmp(packet.m_mac_addr, mac_address, IFHWADDRLEN) != 0) {
			dprintf(D_FULLDEBUG, "Invalid offer mac address.\n");
			continue;
		}
		if (memcmp(packet.m_magic, magic_cookie, 4) != 0) {
			dprintf(D_FULLDEBUG, "Invalid magic cookie.\n");
			continue;
		}
		char * iter = NULL, * value, option;
		size_t length;
		bool valid_packet = true;
		while ((iter = packet.iterateOpts(iter, option, length, value))) {
			if (option == '\x35' && length == 1) {
				if (*value == DHCPNAK) {
					// Valid response, but the DHCP server says to get lost.
					return -1;
				} else if (*value != DHCPACK) {
					// Something else; ignore;
					valid_packet = false;
					break;
				}
			}
		}
		if (!valid_packet) continue;
		// This looks like a valid ACK.  Populate the machine ad.
		populate_classad(packet, machine_ad);
		return 0;
	}
	return 1;
}

static int
get_dhcp_socket(const std::string &interface)
{
	// Query socket.
	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd == -1) {
		dprintf(D_ALWAYS, "Unable to create socket for DHCP query (errno=%d, %s).\n", errno, strerror(errno));
		return -1;
	}
	int optval = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &optval, sizeof(optval)) == -1) {
		dprintf(D_ALWAYS, "Unable to enable broadcasting on DHCP query socket (errno=%d, %s).\n", errno, strerror(errno));
		close(fd);
		return -1;
	}
	if (interface.size() && setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, interface.c_str(), interface.size()) == -1) {
		dprintf(D_ALWAYS, "Unable to bind DHCP socket to device %s (errno=%d, %s).\n", interface.c_str(), errno, strerror(errno));
		close(fd);
		return -1;
	}
	struct sockaddr_in broadcast_addr;
	bzero((char *) &broadcast_addr, sizeof(broadcast_addr));
	broadcast_addr.sin_family = AF_INET;
	broadcast_addr.sin_port = htons(68);
	broadcast_addr.sin_addr.s_addr = INADDR_ANY;
	if (-1 == bind(fd, (sockaddr*)&broadcast_addr, sizeof(broadcast_addr))) {
		dprintf(D_ALWAYS, "Unable to bind to broadcast address for DHCP socket (errno=%d, %s)\n", errno, strerror(errno));
		close(fd);
		return -1;
	}
	struct timeval timeout;
	timeout.tv_sec = 3;
	timeout.tv_usec =0;
	if (-1 == setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout))) {
		dprintf(D_ALWAYS, "Unable to set socket receive timeout.\n");
		close(fd);
		return -1;
	}
	return fd;
}

int
dhcp_query(classad::ClassAd &machine_ad)
{
	std::string device_name;
	if (!machine_ad.EvaluateAttrString(ATTR_BRIDGE_NAME, device_name)) {
		device_name = "";
	}
    dprintf(D_FULLDEBUG, "The device that the dhcp socket binds on to is: %s\n", device_name.c_str());
	int fd = get_dhcp_socket(device_name);
	if (fd == -1) return -1; // get_dhcp_socket already logged error.

	// Transaction ID
	uint32_t txid = random();
	machine_ad.InsertAttr(ATTR_DHCP_TXID, (long int)txid, classad::Value::NO_FACTOR);

	char mac_address[IFHWADDRLEN];
	if (get_mac_address(machine_ad, mac_address)) {
		dprintf(D_ALWAYS, "Unable to get mac address for internal device.\n");
		close(fd);
		return 1;
	}

	// TODO: full timeout.
	// A newly bridged device may send EAGAIN while it is in listening state.
	for (int i = 0; i<3; i++) {
		int retval;
		if ((retval = send_dhcp_discovery(fd, txid, mac_address))) {
			if (retval == EAGAIN) {
				sleep(1);
			} else {
				dprintf(D_ALWAYS, "Unable to send DHCP discovery request (retval=%d).\n", retval);
				close(fd);
				return 1;
			}
		} else {
			break;
		}
	}

	DHCPPacket request_packet(txid, mac_address);
	if (recv_dhcp_offer(fd, txid, mac_address, 4, machine_ad)) {
		dprintf(D_ALWAYS, "Failed to recieve DHCP offer.\n");
		close(fd);
		return 1;
	}
	close(fd);
	return 0;
}

int
dhcp_commit (classad::ClassAd &machine_ad)
{

	std::string interface_name;
	if (!machine_ad.EvaluateAttrString(ATTR_INTERNAL_INTERFACE, interface_name)) {
		dprintf(D_ALWAYS, "Required ClassAd attribute " ATTR_INTERNAL_INTERFACE " is missing.\n");
		return 1;
	}

	char mac_address[IFHWADDRLEN];
	if (get_mac_address(machine_ad, mac_address)) {
		dprintf(D_ALWAYS, "Unable to get mac address for internal device.\n");
		return 1;
	}
	long int txid;
	if (!machine_ad.EvaluateAttrInt(ATTR_DHCP_TXID, txid)) {
		dprintf(D_ALWAYS, "Missing DHCP transaction ID from ClassAd.\n");
		return 1;
	}

	int fd = get_dhcp_socket(interface_name);
	if (fd == -1) return -1;

	for (int i=0; i<2; i++) {

		if (send_dhcp_request(fd, txid, mac_address, machine_ad)) {
			dprintf(D_ALWAYS, "Failed to send DHCP request.\n");
			close(fd);
			return 1;
		}

		int retval;
		if ((retval = recv_dhcp_ack(fd, txid, mac_address, 4, machine_ad))) {
			if (retval == -1) {
				dprintf(D_ALWAYS, "DHCP server rejected our request with a NAK.\n");
			} else {
				dprintf(D_ALWAYS, "Failed to receive DHCP acknowledgement; sleeping and trying again.\n");
				//return 1; // TODO: fix below.
				sleep(2);
				close(fd);
				fd = get_dhcp_socket(interface_name);
				continue;
			}
			close(fd);
			return 1;
		} else {
			close(fd);
			return 0;
		}
	}
	close(fd);
	return 1;
}

int
dhcp_release (classad::ClassAd &machine_ad)
{
	long int txid;
	if (!machine_ad.EvaluateAttrInt(ATTR_DHCP_TXID, txid)) {
		dprintf(D_ALWAYS, "Missing DHCP transaction ID from ClassAd.\n");
		return 1;
	}

	char mac_address[IFHWADDRLEN];
	if (get_mac_address(machine_ad, mac_address)) {
		dprintf(D_ALWAYS, "Unable to get mac address for internal device.\n");
		return 1;
	}

	std::string internal_addr;
	if (!machine_ad.EvaluateAttrString(ATTR_INTERNAL_ADDRESS_IPV4, internal_addr)) {
		dprintf(D_ALWAYS, "Missing IPv4 address on internal interface in ClassAd.\n");
		return 1;
	}
	struct in_addr ciaddr;
	if (inet_pton(AF_INET, internal_addr.c_str(), &ciaddr) != 1) {
		dprintf(D_ALWAYS, "Failed to convert %s to IPv4 address.\n", internal_addr.c_str());
		return 1;
	}
        std::string server_addr;
        if (!machine_ad.EvaluateAttrString(ATTR_DHCP_SERVER, server_addr)) {
                dprintf(D_ALWAYS, "Missing DHCP server address in ClassAd.\n");
                return 1;
        }
        struct in_addr siaddr;
        if (inet_pton(AF_INET, server_addr.c_str(), &siaddr) != 1) {
                dprintf(D_ALWAYS, "Failed to convert %s to IPv4 address.\n", server_addr.c_str());
                return 1;
        }
        std::string gateway_addr;
        if (!machine_ad.EvaluateAttrString(ATTR_DHCP_GATEWAY, gateway_addr)) {
                dprintf(D_ALWAYS, "Missing DHCP relay address in ClassAd.\n");
                return 1;
        }
        struct in_addr giaddr;
        if (inet_pton(AF_INET, gateway_addr.c_str(), &giaddr) != 1) {
                dprintf(D_ALWAYS, "Failed to convert %s to IPv4 address.\n", gateway_addr.c_str());
                return 1;
        }

	DHCPPacket packet(txid, mac_address);
	packet.m_ciaddr = ciaddr;
	packet.m_siaddr = siaddr;
	packet.m_giaddr = giaddr;

	char packet_type = DHCPRELEASE;
	char * req_iter = packet.setOption(NULL, '\x35', 1, &packet_type);
	req_iter = packet.setOption(req_iter, '\x3d', IFHWADDRLEN, packet.m_mac_addr);
	req_iter = packet.setOption(req_iter, '\x32', 4, (char*)&ciaddr.s_addr);
	req_iter = packet.setOption(req_iter, '\x36', 4, (char*)&packet.m_siaddr.s_addr);
	req_iter = packet.setOption(req_iter, '\x3c', 4, "lark");

	std::string device_name;
	if (!machine_ad.EvaluateAttrString(ATTR_BRIDGE_NAME, device_name)) {
		device_name = "";
	}

	int fd = get_dhcp_socket(device_name);
	if (fd == -1) {
		dprintf(D_ALWAYS, "Failed to acquire a socket for DHCP communication.\n");
		return 1;
	}

	struct iovec *iov;
	size_t iov_len;
	packet.makeIOV(iov, iov_len);

	struct msghdr msg;
	bzero(&msg, sizeof(msg));
	struct sockaddr_in dest; bzero(&dest, sizeof(dest));
	dest.sin_family = AF_INET;
	dest.sin_addr.s_addr = siaddr.s_addr;
	dest.sin_port = htons(67);
	msg.msg_name = &dest;
	msg.msg_namelen = sizeof(dest);
	msg.msg_iov = iov;
	msg.msg_iovlen = iov_len;
	if (-1 == sendmsg(fd, &msg, 0)) {
		dprintf(D_ALWAYS, "Failed to send DHCP release packet (errno=%d, %s)\n", errno, strerror(errno));
		close(fd);
		return errno;
	}
	dprintf(D_FULLDEBUG, "Released IP address %s\n", internal_addr.c_str());
	close(fd);
	return 0;
}

int
dhcp_renew(classad::ClassAd &machine_ad)
{
	// The insight here is that the request/ACK sequence for renewing
	// a lease is the same as for committing to a lease.
	return dhcp_commit(machine_ad);
}

