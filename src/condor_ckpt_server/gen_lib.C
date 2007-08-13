/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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
#include "condor_common.h"
#include "gen_lib.h"
#include "constants2.h"

void MakeLongLine(char ch1,
                  char ch2)
{
  int ch_count;

  printf("      ");
  for (ch_count=7; ch_count<124; ch_count++)
    if (ch_count%2)
      printf("%c", ch1);
    else
      printf("%c", ch2);
  printf("\n");
}


void MakeLine(char ch1,
              char ch2)
{
  int ch_count;

  for (ch_count=0; ch_count<79; ch_count++)
    if (ch_count%2)
      printf("%c", ch1);
    else
      printf("%c", ch2);
  printf("\n");
}

extern "C" {
/* XXX This should prolly be documented for why it is here.... */
int
SetSyscalls() { return -1; }
}


