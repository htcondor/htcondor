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
#include "condor_debug.h"
#include "signal2.h"
#include "constants2.h"
#include <iostream>


void BlockSignal(int sig)
{
  sigset_t mask;

  if (sigprocmask(SIG_SETMASK, NULL, &mask) != 0)
    {
	  dprintf(D_ALWAYS, "ERROR: cannot obtain current signal mask\n");
      exit(SIGNAL_MASK_ERROR);
    }
  if (sigaddset(&mask, sig) != 0)
    {
      dprintf(D_ALWAYS, "ERROR: cannot add signal (%d) to signal mask\n",
			  sig);
      exit(SIGNAL_MASK_ERROR);
    }
  if (sigprocmask(SIG_SETMASK, &mask, NULL) != 0)
    {
      dprintf(D_ALWAYS, "ERROR: cannot set new signal mask\n");
      exit(SIGNAL_MASK_ERROR);
    }
}


void UnblockSignal(int sig)
{
  sigset_t mask;

  if (sigprocmask(SIG_SETMASK, NULL, &mask) != 0)
    {
      dprintf(D_ALWAYS, "ERROR: cannot obtain current signal mask\n");
      exit(SIGNAL_MASK_ERROR);
    }
  if (sigdelset(&mask, sig) != 0)
    {
      dprintf(D_ALWAYS, "ERROR: cannot remove signal (%d) from signal mask\n",
			  sig);
      exit(SIGNAL_MASK_ERROR);
    }
  if (sigprocmask(SIG_SETMASK, &mask, NULL) != 0)
    {
	  dprintf(D_ALWAYS, "ERROR: cannot set new signal mask\n");
      exit(SIGNAL_MASK_ERROR);
    }
}


