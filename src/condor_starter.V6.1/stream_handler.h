#ifndef STREAM_HANDLER_H
#define STREAM_HANDLER_H

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

#define STREAM_BUFFER_SIZE 1048576

class StreamHandler : public Service {
public:
	StreamHandler();

	bool Init( const char *filename, const char *streamname, bool is_output );
	int Handler( int fd );
	int GetJobPipe();

private:
	char	buffer[STREAM_BUFFER_SIZE];
	char	filename[_POSIX_PATH_MAX];
	char	streamname[_POSIX_PATH_MAX];
	bool	is_output;
	int	pipe_fds[2];
	int	job_pipe;
	int	handler_pipe;
	int	remote_fd;
	off_t	offset;
};


#endif
