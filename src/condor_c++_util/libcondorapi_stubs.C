#include "condor_common.h"

BEGIN_C_DECLS
char* param(const char *str)
{
	if(strcmp(str, "LOG") == 0) {
		return strdup(".");
	}

	return NULL;
}
END_C_DECLS

