#ifndef DIRECTORY_H
#define DIRECTORY_H

#if defined (ULTRIX42) || defined(ULTRIX43)	/* _POSIX_SOURCE should have taken care of this, */
#define __POSIX			/* but for some reason the ULTRIX version of dirent.h */
#endif					/* is not POSIX conforming without it... */
#include <dirent.h>


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
