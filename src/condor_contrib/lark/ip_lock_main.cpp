
#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"

#include <stdio.h>
#include <unistd.h>

#include "ip_lock.h"

#include <arpa/inet.h>

int main(int argc, char *argv[])
{
	config();
	dprintf_set_tool_debug("TOOL", 0);

	if (argc != 2)
	{
		printf("Usage: %s network/prefix\n", argv[0]);
		return 1;
	}
	dprintf(D_ALWAYS, "Trying to lock IP for network/prefix %s\n", argv[1]);
	IPLock lock(argv[1]);
	condor_sockaddr addr;
	if (!lock.Lock(addr))
	{
		dprintf(D_ALWAYS, "Unable to lock an IP address.\n");
	}
	else
	{
		dprintf(D_ALWAYS, "Locked IP address: %s\n", addr.to_ip_string().Value());
		sleep(10);
	}
	return 0;
}
