#ifndef CONDOR_FILE_FD_H
#define CONDOR_FILE_FD_H

#include "condor_file_local.h"

/*
This file object opens a file locally on an
existing file descriptor.  Apart from the url
type it understands, this object is entirely
identical to CondorFileLocal.
*/

class CondorFileFD : public CondorFileLocal {
public:
	CondorFileFD();
	virtual ~CondorFileFD();
	virtual int open( const char *url, int flags, int mode );
	virtual int close();
};

#endif
