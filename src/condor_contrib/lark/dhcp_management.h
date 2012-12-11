
#ifndef __dhcp_management_h_
#define __dhcp_management_h_

namespace classad {
	class ClassAd;
}

/*
 * Query a DHCP server based on the information found in the machine ad.
 */
int
dhcp_query(classad::ClassAd &machine_ad);

/*
 * Try to commit to a DHCP offer given by a server.
 */
int
dhcp_commit(classad::ClassAd &machine_ad);

/*
 * Try to renew a DHCP lease based on the information found in the machine ad
 */
int
dhcp_lease(classad::ClassAd &machine_ad);

/*
 * Release a DHCP lease.
 */
int
dhcp_release(classad::ClassAd &machine_ad);

#endif

