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

