
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
signals are allowed.  These can be used both by Condor code and
by user code to control checkpointing but still allow other 
signals to be handled.
<p>
Both of these sets of functions occur in matched pairs.  For example, two calls
to _condor_ckpt_disable() must be followed by two calls to
enable() before signals are enabled again.
<p>
WARNING WARNING WARNING: This implementation is not yet complete.
_condor_signals_disable() simply calls the ckpt version to disable 
ckpting.  To do this correctly, two disable counts (one for ckpt, one for all
signals) need to be kept and should be combined with the user's explicit signal
mask to get the resulting correct signal mask.
*/

#include "condor_common.h"

BEGIN_C_DECLS

void	_condor_signals_disable();
void	_condor_signals_enable();

void	_condor_ckpt_disable();
void	_condor_ckpt_enable();

END_C_DECLS

#endif /* _CONDOR_SIGNALS_CONTROL_H */

