
#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"

#include <stdio.h>
#include <unistd.h>

#include "ip_lock.h"

#include <arpa/inet.h>

int main(int argc, char *argv[])
{
	Termlog = 1;
	param_functions *p_funcs = get_param_functions();
	dprintf_config ("TOOL", p_funcs);

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
