
#include "condor_common.h"
#include "sysapi.h"
#include "sysapi_externs.h"

void
sysapi_last_xevent(void)
{
	sysapi_internal_reconfig();

	/* XXX why isn't _sysapi_last_x_event a time_t? */
	_sysapi_last_x_event = (int)time(NULL);
}
