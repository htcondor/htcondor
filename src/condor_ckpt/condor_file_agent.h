
#ifndef CONDOR_FILE_AGENT_H
#define CONDOR_FILE_AGENT_H

#include "condor_file_local.h"

/**
This object takes an existing CondorFile and arranges for
the entire thing to be moved to a local temp file, where
it can be accessed quickly.
<p>
When the file is suspended, it is put back in its original
place.  When the file is resumed, it is retreived again.
<p>
<b>Notice:</b>:
<dir>
<li> This module uses tmpfil() to get a temporary file.  It
be using Todd's file transfer object, which was not available
at press time.
<li> This module still has some unsolved bugs, so its use
is disabled in file_state.C
</dir>
*/

class CondorFileAgent : public CondorFileLocal {
public:
	CondorFileAgent( CondorFile *f );
	virtual ~CondorFileAgent();

	virtual int open( const char *url, int flags, int mode );
	virtual int close();

private:
	void open_temp();
	void close_temp();
	void pull_data();
	void push_data();

	CondorFile *original;
	char local_name[_POSIX_PATH_MAX];
};

#endif

