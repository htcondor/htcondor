#include "condor_common.h"
#include "condor_ast.h"
#include <time.h>

void
evalFromEnvironment (char *name, EvalResult *val)
{
	if (strcmp (name, "CurrentTime") == 0)
	{
		time_t	now = time (NULL);
		if (now == (time_t) -1)
		{
			val->type = LX_ERROR;
			return;
		}
		val->type = LX_INTEGER;
		val->i = (int) now;
		return;
	}

	val->type = LX_UNDEFINED;
	return;
}	
