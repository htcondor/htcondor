#include "condor_common.h"
#include "domain.h"

static char _FileName_[] = __FILE__;

namespace classad {

ClassAdDomainManager::
ClassAdDomainManager ()
{
	schemaArray.fill (NULL);
}


ClassAdDomainManager::
~ClassAdDomainManager ()
{
	int i, j = schemaArray.getsize();

	for (i = 0; i < j; i++) {
		delete schemaArray[i];
	}
}


void ClassAdDomainManager::
GetDomainSchema (char *domainName, int &index, StringSpace *&schema)
{
	index = domainNameSpace.getCanonical (domainName);
	if (index == -1) {
		schema = NULL;		// some problem
		return;
	}

	schema = schemaArray[index];
	if (schema == NULL) {
		schema = new StringSpace;
		if (schema == NULL) {
			EXCEPT("Out of memory");
		}
		schemaArray[index] = schema;
	}
}

} // namespace classad
