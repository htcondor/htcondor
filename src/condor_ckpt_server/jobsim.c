#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>


int main(void)
{
  u_long count;
  int    oc;
  double x;
  double y;
  double pi;
  double total;
  u_long in_circ=0;

  srand((unsigned int) time(NULL));
  total = 0.0;
  for (oc=0; oc<100; oc++)
    {
      for (count=0; count<100000000; count++)
        {
          x = rand()/((float) RAND_MAX);
          y = rand()/((float) RAND_MAX);
          if (x*x+y*y <= 1.0)
    	    in_circ++;
	}
      pi = 4.0 * (double) in_circ/((double) count);
      printf("Approximation #%d: %1.7f\n", oc+1, pi);
      total += pi;
    }
  printf("MC approximation of pi = %1.7f\n", total/100);
  return 0;
}
