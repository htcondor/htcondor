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
#include "condor_common.h"
#include "gen_lib.h"
#include "constants2.h"


void MakeLongLine(char ch1,
                  char ch2)
{
  int ch_count;

  cout << setw(6) << " ";
  for (ch_count=7; ch_count<124; ch_count++)
    if (ch_count%2)
      cout << ch1;
    else
      cout << ch2;
  cout << endl;
}


void MakeLine(char ch1,
              char ch2)
{
  int ch_count;

  for (ch_count=0; ch_count<79; ch_count++)
    if (ch_count%2)
      cout << ch1;
    else
      cout << ch2;
  cout << endl;
}


int OTStrLen(const char* s_ptr,
	     int         max)
{
  char* ptr;
  int   count;

  ptr = (char*) s_ptr;
  count = 0;
  while ((count < max) && (*ptr != '\0'))
    {
      count++;
      ptr++;
    }
  return count;
}


void PrintTime(ofstream& outfile)
{
  time_t current_time;
  char*  time_ptr;
  char   print_time[26];

  current_time = time(NULL);
  time_ptr = ctime(&current_time);
  if (time_ptr != NULL)
    {
      strncpy(print_time, time_ptr, 26);
      print_time[19] = '\0';
      outfile << print_time+4;
    }
  else
    outfile << "** CANNOT ACCESS TIME **";
}


extern "C" {
/* XXX This should prolly be documented for why it is here.... */
int
SetSyscalls() { return -1; }
}


