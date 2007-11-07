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

sigset_t	_condor_signals_disable(void);
void		_condor_signals_enable( sigset_t mask );

void		_condor_ckpt_disable(void);
void		_condor_ckpt_enable(void);

int		_condor_ckpt_is_disabled(void);
int		_condor_ckpt_is_deferred(void);
void		_condor_ckpt_defer( int sig );

END_C_DECLS

#endif /* _CONDOR_SIGNALS_CONTROL_H */











