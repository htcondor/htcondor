
#include "condor_file_agent.h"
#include "condor_syscall_mode.h"
#include "condor_debug.h"
#include "condor_error.h"

#define KB 1024
#define TRANSFER_BLOCK_SIZE (512*KB)

static char buffer[TRANSFER_BLOCK_SIZE];

CondorFileAgent::CondorFileAgent( CondorFile *file )
{
	original = file;
}

CondorFileAgent::~CondorFileAgent()
{
	delete original;
}

int CondorFileAgent::open( const char *path, int flags, int mode )
{
	// Open the original file
	int result = original->open(path,flags,mode);
	if(result<0) return -1;

	// Make my info match
	strcpy(name,original->get_name());
	readable = original->is_readable();
	writeable = original->is_writeable();
	size = original->get_size();

	// And make the local copy
	open_temp();
	pull_data();

	return fd;
}

int CondorFileAgent::close()
{
	push_data();
	close_temp();
	return original->close();
}

void CondorFileAgent::checkpoint()
{
	push_data();
	original->checkpoint();
}

void CondorFileAgent::suspend()
{
	push_data();
	close_temp();
	original->suspend();
}

void CondorFileAgent::resume( int count )
{
	if( resume_count==count ) return;
	original->resume( count );
	open_temp();
	pull_data();
	resume_count = count;
}

void CondorFileAgent::open_temp()
{
	int scm = SetSyscalls(SYS_LOCAL|SYS_UNMAPPED);
	tmpnam(local_name);
	SetSyscalls(scm);

	dprintf(D_ALWAYS,"CondorFileAgent: %s is local copy of %s %s\n",
		local_name, original->get_kind(), original->get_name() );

	scm = SetSyscalls(SYS_LOCAL|SYS_UNMAPPED);
	fd = ::open( local_name, O_RDWR|O_CREAT|O_TRUNC, 0700 );
	if(fd<0) _condor_error_retry("Couldn't create temporary file '%s'!",local_name);
	SetSyscalls(scm);
}

void CondorFileAgent::close_temp()
{
	int scm = SetSyscalls(SYS_LOCAL|SYS_UNMAPPED);
	::close(fd);
	SetSyscalls(scm);

	fd = -1;

	dprintf(D_ALWAYS,"CondorFileAgent: %s is closed.\n",local_name);
}

void CondorFileAgent::pull_data()
{
	if(!original->is_readable()) return;

	dprintf(D_ALWAYS,"CondorFileAgent: Loading %s with %s %s\n",
		local_name, original->get_kind(), original->get_name() );

	int scm = SetSyscalls(SYS_LOCAL|SYS_UNMAPPED);

	int pos=0,chunk=0,result=0;

	do {
		chunk = original->read(pos,buffer,TRANSFER_BLOCK_SIZE);
		if(chunk<0) _condor_error_retry("CondorFileAgent: Couldn't read from '%s'",name);

		if(chunk==0) break;

		result = ::write(fd,buffer,chunk);
		if(result<0) _condor_error_retry("CondorFileAgent: Couldn't write to '%s'",local_name);
		
		pos += chunk;
	} while(chunk==TRANSFER_BLOCK_SIZE);

	SetSyscalls(scm);
}

void CondorFileAgent::push_data()
{
	if(!original->is_writeable()) return;

	dprintf(D_ALWAYS,"CondorFileAgent: Putting %s back into %s %s.\n",
		local_name, original->get_kind(), original->get_name());

	int scm = SetSyscalls(SYS_LOCAL|SYS_UNMAPPED);

	::lseek(fd,0,SEEK_SET);

	int pos=0,chunk=0,result=0;

	do {
		chunk = ::read(fd,buffer,TRANSFER_BLOCK_SIZE);
		if(chunk<0) _condor_error_retry("CondorFileAgent: Couldn't read from '%s'",local_name);

		if(chunk==0) break;

		result = original->write(pos,buffer,chunk);
		if(result<0) _condor_error_retry("CondorFileAgent: Couldn't write to '%s'",name);
			
		pos += chunk;
	} while(chunk==TRANSFER_BLOCK_SIZE);

	SetSyscalls(scm);
}
