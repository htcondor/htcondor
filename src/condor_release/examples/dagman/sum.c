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

