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
#include <iostream.h>
#include <string.h>


void StripFilename(char* pathname)
{
  int s_index, len, count;

  len = strlen(pathname);
  s_index = len-1;
  while ((s_index >= 0) && (pathname[s_index] != '/'))
    s_index--;
  if (s_index >= 0)
    {
      s_index++;
      count = 0;
      while (pathname[s_index] != '\0')
	pathname[count++] = pathname[s_index++];
      pathname[count] = '\0';
    }
}


int main(void)
{
  char tfn1[100]="heretic.zip";
  char tfn2[100]="~tsao/private/cs736/chkpt_shadow/heretic.zip";

  StripFilename(tfn1);
  StripFilename(tfn2);
  cout << "Test file name 1: " << tfn1 << endl;
  cout << "Test file name 2: " << tfn2 << endl;
}
