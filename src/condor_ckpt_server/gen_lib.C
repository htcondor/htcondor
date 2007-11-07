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


