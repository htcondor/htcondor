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
#include "imds2.h"
#include "fileindex2.h"
#include "network2.h"
#include <stdio.h>


int main(void)
{
  IMDS imds;
  int filenum[14] = { 108, 113, 104, 111, 102, 103, 101, 107, 106, 105, 
		      112, 109, 110, 114 };
  char filename[100];
  char* path[3] = { "grumpy",
		    "damsel",
		    "sun12" };
  struct hostent* h;
  struct in_addr machine_IP;
  int machine, count;
  
  for (machine=0; machine<3; machine++)
    {
      h = gethostbyname(path[machine]);
      memcpy((char*) &machine_IP, (char*) h->h_addr, sizeof(struct in_addr));
      for (count=0; count<14; count++)
	{
	  sprintf(filename, "%d", filenum[count]);
	  imds.AddFile(machine_IP, "tsao", filename, filenum[count]);
	}
    }
  imds.DumpIndex();
  return 0;
}
