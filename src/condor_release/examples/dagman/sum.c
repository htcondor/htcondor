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
/*
  total.c

  This program looks for command-line arguments which give the
  filenames it should use for input.  For each file given, final.c
  will open the file, read out a number, and add that number to its
  total.  After all the files have been read, the total is printed to
  STDOUT. 
*/

#include <stdio.h>

main( int argc, char* argv[] )
{
  int i;
  char* filename;
  FILE* fp;
  int total = 0;
  int val;

  for( i=1; i<argc; i++ ) {
    filename = argv[i];
    fp = fopen( filename, "r" );
    if( !fp ) {
      fprintf( stderr, "Error: can't open file: %s\n", filename );
      exit( 1 );
    }
    fscanf( fp, "%d", &val );
    total += val;
    fclose( fp );
  }
  printf( "%d\n", total );
  exit(0);
}

