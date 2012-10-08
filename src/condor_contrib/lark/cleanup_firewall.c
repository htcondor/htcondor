
#include "stdio.h"

#include "network_manipulation.h"

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		printf("Usage: %s CHAIN_NAME\n", argv[0]);
		return 1;
	}
	return cleanup_chain(argv[1]);
}
