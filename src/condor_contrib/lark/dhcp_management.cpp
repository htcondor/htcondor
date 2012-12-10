
#include "condor_common.h"

#include <errno.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <net/if.h>

#include "condor_debug.h"
#include <classad/classad.h>

#include "network_configuration.h"
#include "address_selection.h"

#include "dhcp_management.h"

#define INET_LEN 4

static char dhcp_header[] = {'\x02', '\x01', '\x06', '\x00'};
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
		memcpy(m_header, dhcp_header, 4);
		bzero(m_padding_202, 202);
		}

	int makeIOV(struct iovec *&iov, size_t &iov_len)
	{
		// See Figure 1 of RFC2131 for packet layout
		iov_len = 13;
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
		m_iov[10].iov_base = &m_options;
		m_iov[10].iov_len = 1024;

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
	ssize_t getOptionLength() {return m_length >= 0 ? m_length - 236 : -1;}

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

private:
	char m_padding_202[202];
	struct iovec m_iov[13];

};

static int
get_mac_address(const classad::ClassAd &machine_ad, char result[IFHWADDRLEN])
{
	std::string interface_name;
	if (!machine_ad.EvaluateAttrString(ATTR_EXTERNAL_INTERFACE, interface_name)) {
		dprintf(D_ALWAYS, "Required ClassAd attribute " ATTR_EXTERNAL_INTERFACE " is missing.\n");
		return 1;
	}
	if (interface_name.length() > IFNAMSIZ) {
		dprintf(D_ALWAYS, "External interface name (%s) is too long.\n", interface_name.c_str());
		return 1;
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

	return 0;
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
	iter = packet.setOption(iter, '\xff', 0, NULL);

	struct iovec *iov;
	size_t iov_len;
	packet.makeIOV(iov, iov_len);

	struct msghdr msg;
	bzero(&msg, sizeof(msg));
	struct sockaddr_in dest;
	dest.sin_family = AF_INET;
	dest.sin_addr.s_addr = INADDR_BROADCAST;
	dest.sin_port = htons(67);
	msg.msg_iov = iov;
	msg.msg_iovlen = 18;
	if (-1 == recvmsg(fd, &msg, 0)) {
		dprintf(D_ALWAYS, "Failed to send DHCP discover packet (errno=%d, %s)\n", errno, strerror(errno));
		return 1;
	}
	return 0;
}

static int
recv_dhcp_offer(int fd, DHCPPacket &request_packet, unsigned secs)
{
	DHCPPacket packet(request_packet.m_txid, request_packet.m_mac_addr);

	struct msghdr msg;
	ssize_t packet_size;
	time_t starttime = time(NULL);
	while (time(NULL) - starttime <= secs) {
		struct iovec *iov;
		size_t iov_len;
		packet.makeIOV(iov, iov_len);
		msg.msg_iov = iov;
		msg.msg_iovlen = iov_len;
		packet_size = recvmsg(fd, &msg, 0);
		if (packet_size == -1) {
			if (errno == EINTR) continue;
			dprintf(D_ALWAYS, "Error when waiting for DHCP offer (errno=%d, %s).\n", errno, strerror(errno));
			break;
		}
		if (packet_size <= 236) {
			// packet is too small to be valid.
			continue;
		}
		if (memcmp(dhcp_header, packet.m_header, 4) != 0) {
			dprintf(D_FULLDEBUG, "Invalid DHCP offer header.\n");
			continue;
		}
		if (packet.m_txid != request_packet.m_txid) {
			dprintf(D_FULLDEBUG, "Invalid offer TXID: %d\n", packet.m_txid);
			continue;
		}
		if (memcmp(packet.m_mac_addr, request_packet.m_mac_addr, IFHWADDRLEN) != 0) {
			dprintf(D_FULLDEBUG, "Invalid offer mac address.\n");
			continue;
		}
		// At this point, we now believe this is a valid offer.  Populate the corresponding request packet.
		packet.computeLength(packet_size);
		char * iter;
		char option;
		size_t length;
		char * value;
		char packet_type = DHCPREQUEST;
		char * req_iter = request_packet.setOption(NULL, '\x35', 1, &packet_type);
		req_iter = request_packet.setOption(req_iter, '\x3d', IFHWADDRLEN, request_packet.m_mac_addr);
		bool valid_packet = true;
		while ((iter = packet.iterateOpts(iter, option, length, value))) {
			// Note that we ignore most of the info in the offer.
			// We delay utilizing any information until the ACK.
			if (option == '\x35' && *value != DHCPOFFER) {
				valid_packet = false;
				break;
			}
		}
		if (valid_packet == true) {
			req_iter = request_packet.setOption(req_iter, '\x32', 4, (char*)&packet.m_yiaddr.s_addr);
			req_iter = request_packet.setOption(req_iter, '\x36', 4, (char*)&packet.m_siaddr.s_addr);
			request_packet.m_ciaddr.s_addr = packet.m_ciaddr.s_addr;
			request_packet.m_yiaddr.s_addr = INADDR_ANY;
			request_packet.m_siaddr.s_addr = packet.m_siaddr.s_addr;
			request_packet.m_giaddr.s_addr = packet.m_giaddr.s_addr;
			return 0;
		}
	}
	return 1;
}

static int
send_dhcp_request(int fd, DHCPPacket &packet)
{
	struct iovec *iov;
	size_t iov_len;
	packet.makeIOV(iov, iov_len);

	struct msghdr msg;
	bzero(&msg, sizeof(msg));
	struct sockaddr_in dest;
	dest.sin_family = AF_INET;
	dest.sin_addr.s_addr = INADDR_BROADCAST;
	dest.sin_port = htons(67);
	msg.msg_iov = iov;
	msg.msg_iovlen = 18;
	if (-1 == recvmsg(fd, &msg, 0)) {
		dprintf(D_ALWAYS, "Failed to send DHCP discover packet (errno=%d, %s)\n", errno, strerror(errno));
		return 1;
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
		packet.makeIOV(iov, iov_len);
		msg.msg_iov = iov;
		msg.msg_iovlen = iov_len;
		packet_size = recvmsg(fd, &msg, 0);
		if (packet_size == -1) {
			if (errno == EINTR) continue;
			dprintf(D_ALWAYS, "Error when waiting for DHCP ACK (errno=%d, %s).\n", errno, strerror(errno));
			break;
		}
		if (packet_size <= 236) {
			// packet is too small to be valid.
			continue;
		}
		if (memcmp(dhcp_header, packet.m_header, 4) != 0) {
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
		char ip_address[INET_ADDRSTRLEN+1];
		if (inet_ntop(AF_INET, &packet.m_yiaddr, ip_address, INET_ADDRSTRLEN)) {
			machine_ad.InsertAttr(ATTR_INTERNAL_ADDRESS_IPV4, ip_address);
		}
		iter = NULL;
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
		return 0;
	}
	return 1;
}

int
dhcp_query(classad::ClassAd &machine_ad)
{
	// Query socket.
	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd == -1) {
		dprintf(D_ALWAYS, "Unable to create socket for DHCP query (errno=%d, %s).\n", errno, strerror(errno));
		return errno;
	}
	int optval = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &optval, sizeof(optval)) == -1) {
		dprintf(D_ALWAYS, "Unable to enable broadcasting on DHCP query socket (errno=%d, %s).\n", errno, strerror(errno));
		close(fd);
		return errno;
	}
	struct sockaddr_in broadcast_addr;
	bzero((char *) &broadcast_addr, sizeof(broadcast_addr));
	broadcast_addr.sin_family = AF_INET;
	broadcast_addr.sin_port = htons(68);
	broadcast_addr.sin_addr.s_addr = INADDR_ANY;
	if (-1 == bind(fd, (sockaddr*)&broadcast_addr, sizeof(broadcast_addr))) {
		dprintf(D_ALWAYS, "Unable to bind to broadcast address for DHCP socket (errno=%d, %s)\n", errno, strerror(errno));
		close(fd);
		return errno;
	}
	struct timeval timeout;
	timeout.tv_sec = 3;
	timeout.tv_usec =0;
	if (-1 == setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout))) {
		dprintf(D_ALWAYS, "Unable to set socket receive timeout.\n");
		close(fd);
		return errno;
	}

	// Transaction ID
	uint32_t txid = random();

	char mac_address[IFHWADDRLEN];
	if (get_mac_address(machine_ad, mac_address)) {
	}

	if (send_dhcp_discovery(fd, txid, mac_address)) {
		dprintf(D_ALWAYS, "Unable to send DHCP discovery request.");
		close(fd);
		return 1;
	}

	DHCPPacket request_packet(txid, mac_address);
	if (recv_dhcp_offer(fd, request_packet, 4)) {
		dprintf(D_ALWAYS, "Failed to recieve DHCP offer.\n");
		close(fd);
		return errno;
	}

	if (send_dhcp_request(fd, request_packet)) {
		dprintf(D_ALWAYS, "Failed to send DHCP request.\n");
		close(fd);
		return errno;
	}

	if (recv_dhcp_ack(fd, txid, mac_address, 4, machine_ad)) {
		dprintf(D_ALWAYS, "Failed to receive DHCP acknowledgement.\n");
		close(fd);
		return errno;
	}

	return 0;
}

