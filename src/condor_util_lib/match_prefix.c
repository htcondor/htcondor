#include "condor_common.h"

int
match_prefix(const char *s1, const char *s2)
{
	int	s1l = strlen(s1);
	int s2l = strlen(s2);
	int min = (s1l < s2l) ? s1l : s2l;

	// return true if the strings match (i.e., strcmp() returns 0)
	if (strncmp(s1, s2, min) == 0)
		return 1;

	return 0;
}
