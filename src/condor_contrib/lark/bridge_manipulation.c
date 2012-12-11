
#include "condor_config.h"

#include <errno.h>
#include <string.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <linux/sockios.h>

#include "condor_debug.h"

int
create_bridge(const char * bridge_name)
{
	char brname[IFNAMSIZ];
	strncpy(brname, bridge_name, IFNAMSIZ);

	int fd;
	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		dprintf(D_FULLDEBUG, "Unable to create socket for bridge manipulation (errno=%d, %s).\n", errno, strerror(errno));
		return errno;
	}
	int ret = ioctl(fd, SIOCBRADDBR, brname);
	if (ret < 0 && errno != EEXIST) {
		dprintf(D_FULLDEBUG, "Error when creating bridge %s (errno=%d, %s).\n", bridge_name, errno, strerror(errno));
		close(fd);
		return errno;
	}
	close(fd);
	return 0;
}

int
add_interface_to_bridge(const char * bridge_name, const char * dev)
{
	struct ifreq ifr;
	strncpy(ifr.ifr_name, bridge_name, IFNAMSIZ);

	int ifindex = if_nametoindex(dev);
	if (ifindex == 0) {
		return ENODEV;
	}

	int fd;
	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		dprintf(D_FULLDEBUG, "Unable to create socket for bridge manipulation (errno=%d, %s).\n", errno, strerror(errno));
		return errno;
	}
	ifr.ifr_ifindex = ifindex;
	int ret = ioctl(fd, SIOCBRADDIF, &ifr);
	if (ret < 0 && errno != EBUSY) {
		dprintf(D_FULLDEBUG, "Error when adding interface %s to bridge %s (errno=%d, %s).\n", dev, bridge_name, errno, strerror(errno));
		close(fd);
		return errno;
	}
	close(fd);
	return 0;
}

