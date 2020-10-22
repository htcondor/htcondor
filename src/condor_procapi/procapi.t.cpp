/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

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

	pidenvid_init(&penvid);
	
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
	
