#define _POSIX_SOURCE

#include "condor_common.h"
#include "condor_debug.h"
#include "directory.h"

static char *_FileName_ = __FILE__;     /* Used by EXCEPT (see except.h)     */


Directory::Directory( const char *name )
{
	dirp = opendir( name );
	if( dirp == NULL ) {
		EXCEPT( "Can't open directory \"%s\"", name );
	}
}

Directory::~Directory()
{
	(void)closedir( dirp );
}

void
Directory::Rewind()
{
	rewinddir( dirp );
}

char *
Directory::Next()
{
	struct dirent *dirent;

	while( dirent = readdir(dirp) ) {
		if( strcmp(".",dirent->d_name) == MATCH ) {
			continue;
		}
		if( strcmp("..",dirent->d_name) == MATCH ) {
			continue;
		}
		return dirent->d_name;
	}
	return NULL;
}
