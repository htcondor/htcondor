/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

 

#define _POSIX_SOURCE

#include <stdio.h>
#include "environ.h"

main( int argc, char *argv[] )
{
	char	buf[1024];
	char	**vec;
	char	*answer;
	Environ	foo;
	Environ bar;
	FILE	*fp;

	if( argc != 2 ) {
		printf( "Usage: %s filename\n" );
		return 1;
	}

	if( (fp=fopen(argv[1],"r")) == NULL ) {
		perror( argv[1] );
		return 1;
	}

	foo.add_string( "FOO=bar" );
	foo.add_string( "BAR=glarch" );
	foo.display();

	while( fgets(buf,sizeof(buf),fp) ) {
		bar.add_string( buf );
	}
	vec = bar.get_vector();
	for( ; *vec; vec++ ) {
		printf( "%s\n", *vec );
	}

	while( gets(buf) ) {
		answer = bar.getenv(buf);
		printf( "Variable \"%s\" = \"%s\"\n", buf, answer ? answer : "(NULL)" );
	}
	return 0;
}
