#include "condor_common.h"
#include "condor_config.h"

extern "C" {
	int free_fs_blocks( const char* );
	int	calc_phys_memory();
}


int
main(int argc, char* argv[]) 
{
	config(0);
	if( argc > 1 ) {
		printf( "Free space in %s: %d\n", argv[1], free_fs_blocks(argv[1]) );
	}
	printf( "Free memory: %d\n", calc_phys_memory() );
	return 0;

}



