#include "condor_common.h"
#include "stdio.h"
#include "string.h"
#include "totals.h"

Totals::
Totals ()
{
	descsUsed = 0;
}


Totals::
~Totals ()
{
	for (int i = 0; i < descsUsed; i++) {
		delete [] totalNodes[i].data;
	}
}

void Totals::
setSummarySize (int size)
{
	dataSize = size;
}

void *Totals::
getTotals (char *arch, char *opsys)
{
	static	char	buffer[_descSize];
	int		n;
	
	sprintf (buffer, "%s/%s", arch, opsys);
	for (int i = 0; i < descsUsed; i++) {
		if (strcmp (buffer, totalNodes[i].archOs) == 0) {
			return totalNodes[i].data;
		}
	}
	n = descsUsed;
	if (++descsUsed >= _maxDescs) {
		fprintf (stderr, "Internal error:  Cannot handle more than %d arch/os"
			"combination\n");
		exit (1);
	}
	strcpy (totalNodes[n].archOs, buffer);
	if ((totalNodes[n].data = (void *) new char [dataSize]) == NULL) {
		fprintf (stderr, "Out of memory\n");
		exit (1);
	}
	return (memset (totalNodes[n].data, dataSize, '\0'));
}

void *Totals::
getTotals (int i)
{
	if (i < 0 || i >= descsUsed) {
		return NULL;
	}
	return (void *) totalNodes[i].data;
}
