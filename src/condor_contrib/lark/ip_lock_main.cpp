
#include "stdio.h"

#include "ip_lock.h"

#include <arpa/inet.h>

int main(int argc, char *argv[])
{
	if (argc != 3)
	{
		printf("Usage: %s network prefix\n", argv[0]);
		return 1;
	}
	in_addr n_addr, p_addr;
	// TODO: if result is -1, errno is set.
	if (0 == inet_pton(AF_INET, argv[1], &n_addr))
	{
		printf("Invalid IPv4 address: %s\n", argv[1]);
		return 1;
	}
	if (0 == inet_pton(AF_INET, argv[2], &p_addr))
	{
		printf("Invalid IPv4 prefix: %s\n", argv[2]);
		return 1;
	}
	IPIterator it(n_addr, p_addr);

	in_addr result;
	int idx = 0;
	while ((result.s_addr = it.Next()))
	{
		if (idx++ == 300) break;
		char address[INET_ADDRSTRLEN];
		// TODO: check errno as appropriate, print message.
		const char * address_ptr = inet_ntop(AF_INET, &result, address, INET_ADDRSTRLEN);
		if (address_ptr)
		{
			printf("Possible address: %s\n", address_ptr);
		}
	}
	return 0;
}
