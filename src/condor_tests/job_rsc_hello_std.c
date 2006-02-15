/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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
#define _POSIX_SOURCE

#if defined(HPUX)
/*#include "condor_common.h"*/
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#else
#include "condor_common.h"
/*#include <stdio.h>*/
/*#include <fcntl.h>*/
/*#include <errno.h>*/
/*#include <stdlib.h>*/
/*#include <unistd.h>*/
/*#include <string.h>*/
#endif

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

int buf_eq( const char *buf1, const char *buf2, unsigned int size );

char	Hello[] = "Hello World\n";
char	Buf1[1024];
char	Buf2[1024] = "******************************";

int
main()
{
	int		rval;
	int		fd;

#undef WAIT_FOR_DEBUGGER
#if defined(WAIT_FOR_DEBUGGER)
	int		wait_up = 1;
	while( wait_up )
		;
#endif

	Buf1[0] = Buf2[0];

		/* Write "Hello World" to stdout */
	if( (rval=write(1,Hello,strlen(Hello))) < 0 ) {
		exit( __LINE__ );
	}

		/* Read line from stdin using read routine */
	if( (rval=read(0,Buf1,sizeof(Buf1))) < 0 ) {
		exit( __LINE__ );
	}
	if( lseek(0,0,0) < 0 ) {
		exit( __LINE__ );
	}

		/* Read line from stdin using stdio routine */
	if( fgets(Buf2,sizeof(Buf2),stdin) == NULL ) {
		exit( __LINE__ );
	}
	rewind( stdin );

		/* Check the buffers are equal */
	if( !buf_eq(Buf1,Buf2,rval) ) {
		exit( __LINE__ );
	}

		/* Read line from stdin using stdio routine */
	if( fgets(Buf1,sizeof(Buf1),stdin) == NULL ) {
		exit( __LINE__ );
	}
	rewind( stdin );

		/* Check the buffers are equal */
	if( !buf_eq(Buf1,Buf2,rval) ) {
		exit( __LINE__ );
	}

		/* Echo the line to stdout */
	if( (rval=write(1,Buf1,strlen(Buf1))) < 0 ) {
		exit( __LINE__ );
	}

		/* Create a file named "hello.results" */
	if( (fd=open("hello.results",O_WRONLY|O_CREAT|O_TRUNC,0664)) < 0 ) {
		exit( __LINE__ );
	}

		/* Write "Hello World" to "results" */
	if( (rval=write(fd,Hello,strlen(Hello))) < 0 ) {
		exit( __LINE__ );
	}

		/* Write "Goodby World" to stdout using stdio routine */
	printf( "Goodby World\n" );

		/* Write "Goodby World" to stderr using stdio routine */
	fprintf( stderr, "Goodby World\n" );

	printf( "Normal End Of Job\n" );

	exit( 0 );
}

int
buf_eq( const char *buf1, const char *buf2, unsigned int size )
{
	if( strlen(buf1) != size ) {
		return FALSE;
	}

	if( strlen(buf2) != size ) {
		return FALSE;
	}

	return memcmp( buf1, buf2, size ) == 0;
}
