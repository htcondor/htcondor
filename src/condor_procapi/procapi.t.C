#include "procapi.h"
#include "killfamily.h"
#include <sys/types.h>
#include <unistd.h>

main () {

	ProcAPI p;
	ProcFamily simpsons(getpid(), (priv_state) 1, &p);
	piPTR pi = NULL;
	pid_t fam[128];
	
	for ( int i=0 ; i < 1 ; i++ ) {

		for ( int z=0 ; z<128 ; z++ ) {
			fam[z] = 0;
		}

		printf ("Beginning fork bomb:\n" );

			/* fork bomb yourself */
		for ( int j=0 ; j<100 ; j++ ) {
			if ( fork() == 0 ) {  // child
				sleep ( 20 );
				exit(0);
			}
			else { // parent
			}
		}
		sleep ( 10 );

		p.getPidFamily( getpid(), fam );
		simpsons.takesnapshot();

		printf ( "Run %d: ", i );
		for ( int k=0 ; fam[k] != 0 ; k++ ) {
			p.getProcInfo ( fam[k], pi );
			printf ( "%d ", fam[k] );
		}
		printf ( "\n" );


		int statusp;
		for ( int a=0 ; a<100 ; a++ ) {
			wait ( &statusp );
		}

	}

	delete pi;
}
	
