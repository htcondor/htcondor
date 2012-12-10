
#ifndef __dhcp_management_h_
#define __dhcp_management_h_

namespace classad {
	class ClassAd;
}

#define ATTR_DHCP_SERVER "LarkDHCPServer"
#define ATTR_DHCP_LEASE "LarkDHCPLeaseLifetime"
#define ATTR_DHCP_LEASE_START "LarkDHCPLeaseStart"
#define ATTR_GATEWAY "LarkGateway"
#define ATTR_SUBNET_MASK "LarkSubnetMask"

/*
 * Query a DHCP server based on the information found in the machine ad.
 */
int
dhcp_query(classad::ClassAd &machine_ad);

/*
 * Try to renew a DHCP lease based on the information found in the machine ad
 */
int
dhcp_lease(classad::ClassAd &machine_ad);

#endif

