#include "condor_common.h"
#include "condor_distribution.h"

BEGIN_C_DECLS
char* param(const char *str)
{
	if(strcmp(str, "LOG") == 0) {
		return strdup(".");
	}

	return NULL;
}
END_C_DECLS

template class TransparentSingletonAPI<Distribution>;
