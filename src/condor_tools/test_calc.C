#include "condor_common.h"
#include "condor_config.h"

int
main(int argc, char* argv[]) 
{
	config();
	if( argc > 1 ) {
		printf( "Free space in %s: %d\n", argv[1], sysapi_disk_space(argv[1]) );
	}
	printf( "Free memory: %d\n", sysapi_phys_memory() );
	return 0;

}



