/* 
** Copyright 1986, 1987, 1988, 1989 University of Wisconsin
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the name of the University of
** Wisconsin not be used in advertising or publicity pertaining to
** distribution of the software without specific, written prior
** permission.  The University of Wisconsin makes no representations about
** the suitability of this software for any purpose.  It is provided "as
** is" without express or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN DISCLAIMS ALL WARRANTIES WITH REGARD TO
** THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
** FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF WISCONSIN  BE LIABLE FOR
** ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
** WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
** ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
** OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
** 
** Authors:  Allan Bricker and Michael J. Litzkow,
** 	         University of Wisconsin, Computer Sciences Dept.
** 
*/ 


/*
** This program just computes for an approximate amount of time, then
** exits with status 0.  The approximate time in seconds is specified
** on the command line.  
*/

#include <stdio.h>
#include <unistd.h>

extern int CkptInProgress;

main( argc, argv)
int		argc;
char	*argv[];
{
	int		count;
	int		i;

	switch( argc ) {

	  case 1:
		if( scanf("\n%d", &count) != 1 ) {
			fprintf( stderr, "Input syntax error\n" );
			exit( 1 );
		}
		break;

	  case 2:
		if( (count=atoi(argv[1])) <= 0 ) {
			usage();
		}
		break;
		

	  default:
		usage();

	}

	do_loop( count );
	printf( "Normal End of Job\n" );

	exit( 0 );
}

extern int errno;

do_loop( count )
int		count;
{
	int		i;

	for( i=0; i<count; i++ ) {
		waste_a_second();
		printf( "%d", i );
		fflush( stdout );
		if( write(1," ",1) != 1 ) {
			exit( errno );
		}

		if ( (i % 43) == 0 ) {
		  ckpt();
		}
	}
}


waste_a_second()
{
  int i;
  for(i=0; i < 8000000; i++);
}

usage()
{
	printf( "Usage: loop count\n" );
	exit( 1 );
}

