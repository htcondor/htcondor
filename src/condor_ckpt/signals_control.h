
#ifndef _CONDOR_SIGNALS_CONTROL_H
#define _CONDOR_SIGNALS_CONTROL_H

/*
These two functions control whether asynchronous Condor events
(checkpointing, suspension, etc.) are processed.  An internal
count of the number of times disable() has been called is kept.
To re-enable signals, enable() must be called once for every time
disable() has been called.
*/

#include "condor_common.h"

BEGIN_C_DECLS

void	_condor_signals_disable();
void	_condor_signals_enable();

END_C_DECLS

#endif /* _CONDOR_SIGNALS_CONTROL_H */

