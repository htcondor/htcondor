
#include "condor_common.h"
#include "MyString.h"

int MyStringHash( const MyString &str, int buckets )
{
	return str.Hash()%buckets;
}

