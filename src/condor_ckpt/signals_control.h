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

#ifndef _CONDOR_SIGNALS_CONTROL_H
#define _CONDOR_SIGNALS_CONTROL_H

/**
There are two sets of matched calls here.
<p>
_condor_signals_disable() and enable() control whether signals
of any kind are allowed.  These should only be used to control
critical sections where the Condor code cannot be interrupted
for any reason.
<p>
_condor_ckpt_disable() and enable() control whether the checkpointing
signal is enabled.  If disabled, the signal will be received, but the
handler will set a flag and ignore the signal.  If ckpting is re-enabled,
a ckpt will take place immediately.
<p>
Three functions are provided for the use of the checkpointer.
_condor_ckpt_is_disabled() returns the current depth of disable calls.
_condor_ckpt_defer() sets the state necessary to force a checkpoint
at the next enable.  _condor_ckpt_is_deferred() returns true if
a checkpoint has been requested, but is currently blocked.
<p>
Both of these sets of functions occur in matched pairs.  For example, two calls
to _condor_ckpt_disable() must be followed by two calls to
enable() before signals are enabled again.
*/

#include "condor_common.h"

BEGIN_C_DECLS

sigset_t	_condor_signals_disable();
void		_condor_signals_enable( sigset_t mask );

void		_condor_ckpt_disable();
void		_condor_ckpt_enable();

int		_condor_ckpt_is_disabled();
int		_condor_ckpt_is_deferred();
void		_condor_ckpt_defer( int sig );

END_C_DECLS

#endif /* _CONDOR_SIGNALS_CONTROL_H */











