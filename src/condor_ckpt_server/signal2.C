/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
#include "condor_debug.h"
#include "signal2.h"
#include "constants2.h"
#include <iostream.h>


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


