#include "sysapi.h"

/* 
	this is a temporary file to map all of the old function names to the new
	API system before I go through all of the old code and replace them.
	The sysapi is a comletely c linkaged entity.
*/

/* also not that these return the cooked versions of the functions */

int
calc_phys_memory()
{
	sysapi_phys_memory();
}


int
free_fs_blocks(const char *filename)
{
	sysapi_free_fs_blocks(filename);
}
