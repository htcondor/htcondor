/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#include "condor_common.h"
#include "condor_pidenvid.h"
#include "procapi.h"
#include "killfamily.h"
#include <sys/types.h>
#include <unistd.h>

main () {

	ProcAPI p;
	ProcFamily simpsons(getpid(), (priv_state) 1, &p);
	piPTR pi = NULL;
	pid_t fam[128];
	PidEnvID penvid;
	int fam_status;
	int info_status;

	pidnevid_init(&penvid);
	
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

		p.getPidFamily( getpid(), &penvid, fam, fam_status );
		simpsons.takesnapshot();

		printf ( "Run %d: ", i );
		for ( int k=0 ; fam[k] != 0 ; k++ ) {
			p.getProcInfo ( fam[k], pi, info_status );
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
	
