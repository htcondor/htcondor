
#ifndef CONDOR_FILE_SPECIAL_H
#define CONDOR_FILE_SPECIAL_H

#include "condor_file_local.h"

/**
There are several items which look like files, but which Condor
does not know how to checkpoint.  We will allow the use of things
like pipes and sockets, but we must inhibit checkpointing while
these endpoints exist.

This object blocks the appropriate signals in its constructor
and then resumes those signals in the destructor.
When none of these objects are in the file table, checkpointing
is allowed.
*/

class CondorFileSpecial : public CondorFileLocal {
public:
	CondorFileSpecial();
	virtual ~CondorFileSpecial();

	virtual int attach( int fd );
};

#endif

