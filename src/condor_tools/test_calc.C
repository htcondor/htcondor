#include "condor_common.h"
#include "condor_config.h"

extern "C" {
	int free_fs_blocks( char* );
}


int
main(int argc, char* argv[]) 
{
	config(0);
	printf( "Free space in %s: %d\n", argv[1], free_fs_blocks(argv[1]) );
	return 0;
}



