#ifndef DIRECTORY_H
#define DIRECTORY_H

#if defined (ULTRIX42) || defined(ULTRIX43)
	/* _POSIX_SOURCE should have taken care of this,
		but for some reason the ULTRIX version of dirent.h
		is not POSIX conforming without it...
	*/
#	define __POSIX
#endif

#include <dirent.h>

#if defined(SUNOS41) && defined(_POSIX_SOURCE)
	/* Note that function seekdir() is not required by POSIX, but the sun
	   implementation of rewinddir() (which is required by POSIX) is
	   a macro utilizing seekdir().  Thus we need the prototype.
	*/
	extern "C" void seekdir( DIR *dirp, long loc );
#endif

class Directory
{
public:
	Directory( const char *name );
	~Directory();
	void Rewind();
	char *Next();
private:
	DIR *dirp;
};

#endif
