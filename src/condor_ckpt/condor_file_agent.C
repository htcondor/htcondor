
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

int CondorFileAgent::open( const char *url_in, int flags, int mode )
{
	strcpy(url,url_in);

	// Open the original file
	int result = original->open(url,flags,mode);
	if(result<0) return -1;

	// Make my info match
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

void CondorFileAgent::open_temp()
{
	int scm = SetSyscalls(SYS_LOCAL|SYS_UNMAPPED);
	tmpnam(local_name);
	SetSyscalls(scm);

	dprintf(D_ALWAYS,"CondorFileAgent: %s is local copy of %s\n",
		local_name, original->get_url() );

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

	dprintf(D_ALWAYS,"CondorFileAgent: Loading %s with %s\n",
		local_name, original->get_url() );

	int scm = SetSyscalls(SYS_LOCAL|SYS_UNMAPPED);

	int pos=0,chunk=0,result=0;

	do {
		chunk = original->read(pos,buffer,TRANSFER_BLOCK_SIZE);
		if(chunk<0) _condor_error_retry("CondorFileAgent: Couldn't read from '%s'",original->get_url());

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

	dprintf(D_ALWAYS,"CondorFileAgent: Putting %s back into %s.\n",
		local_name, original->get_url() );

	int scm = SetSyscalls(SYS_LOCAL|SYS_UNMAPPED);

	::lseek(fd,0,SEEK_SET);

	int pos=0,chunk=0,result=0;

	do {
		chunk = ::read(fd,buffer,TRANSFER_BLOCK_SIZE);
		if(chunk<0) _condor_error_retry("CondorFileAgent: Couldn't read from '%s'",local_name);

		if(chunk==0) break;

		result = original->write(pos,buffer,chunk);
		if(result<0) _condor_error_retry("CondorFileAgent: Couldn't write to '%s'",original->get_url());
			
		pos += chunk;
	} while(chunk==TRANSFER_BLOCK_SIZE);

	SetSyscalls(scm);
}
