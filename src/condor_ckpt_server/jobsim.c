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
