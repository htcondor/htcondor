#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>

main()
{
  long time;
  int result;
  struct timeval tv;
  gettimeofday( &tv, NULL );
  time = tv.tv_sec - tv.tv_usec;
  srand( (unsigned int)time );
  result = rand();
  if( result % 2 ) {
    result += 1;
  }
  printf( "%d\n", result );
  exit(0);
}

