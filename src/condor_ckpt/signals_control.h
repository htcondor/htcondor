
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











