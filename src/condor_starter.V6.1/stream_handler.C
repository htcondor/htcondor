
#include "stream_handler.h"
#include "NTsenders.h"

StreamHandler::StreamHandler()
{    
}


bool StreamHandler::Init( const char *fn, const char *sn, bool io )
{
	int flags;
	int result;
	HandlerType handler_mode;

	strcpy(filename,fn);
	strcpy(streamname,sn);
	is_output = io;

	if(is_output) {
		flags = O_CREAT|O_TRUNC|O_WRONLY;
	} else {
		flags = O_RDONLY;
	}

	remote_fd = REMOTE_CONDOR_open(filename,(open_flags_t)flags,0777);
	if(remote_fd<0) {
		dprintf(D_ALWAYS,"Couldn't open %s to stream %s: %s\n",filename,streamname,strerror(errno));
		return false;
	}

	result = daemonCore->Create_Pipe(pipe_fds,false,!is_output,STREAM_BUFFER_SIZE);
	if(result==0) {
		dprintf(D_ALWAYS,"Couldn't create pipe to stream %s: %s\n",streamname,strerror(errno));
		REMOTE_CONDOR_close(remote_fd);
		return false;
	}

	if(is_output) {
		job_pipe = pipe_fds[1];
		handler_pipe = pipe_fds[0];
		handler_mode = HANDLE_READ;
	} else {
		job_pipe = pipe_fds[0];
		handler_pipe = pipe_fds[1];
		handler_mode = HANDLE_WRITE;
	}

	offset = 0;
	daemonCore->Register_Pipe(handler_pipe,"Job I/O Pipe",(PipeHandlercpp)&StreamHandler::Handler,"Stream I/O Handler",this,handler_mode);
	return true;
}

int StreamHandler::Handler( int fd )
{
	int result;
	int actual;

	if(is_output) {
		dprintf(D_SYSCALLS,"StreamHandler: %s: stream ready\n",streamname);
		result = read(handler_pipe,buffer,STREAM_BUFFER_SIZE);
		if(result>0) {
			dprintf(D_SYSCALLS,"StreamHandler: %s: %d bytes available\n",streamname,result);
			REMOTE_CONDOR_lseek(remote_fd,offset,SEEK_SET);
			actual = REMOTE_CONDOR_write(remote_fd,buffer,result);
			if(actual!=result) {
				EXCEPT("StreamHandler: %s: couldn't write to %s: %s (%d!=%d) \n",streamname,filename,strerror(errno),actual,result);
			}
			dprintf(D_SYSCALLS,"StreamHandler: %s: %d bytes written to %s\n",streamname,result,filename);
			offset+=actual;
		} else if(result==0) {
			dprintf(D_SYSCALLS,"StreamHandler: %s: end of stream\n",streamname);
			daemonCore->Cancel_Pipe(handler_pipe);
			daemonCore->Close_Pipe(handler_pipe);
		} else if(result<0) {
			dprintf(D_SYSCALLS,"StreamHandler: %s: unable to read: %s\n",streamname,strerror(errno));
		}
	} else {
		dprintf(D_SYSCALLS,"StreamHandler: %s: stream ready\n",streamname);
		REMOTE_CONDOR_lseek(remote_fd,offset,SEEK_SET);
		result = REMOTE_CONDOR_read(remote_fd,buffer,STREAM_BUFFER_SIZE);
		if(result>0) {
			dprintf(D_SYSCALLS,"StreamHandler: %s: %d bytes read from %s\n",streamname,result,filename);
			actual = write(handler_pipe,buffer,result);
			if(actual>=0) {
				dprintf(D_SYSCALLS,"StreamHandler: %s: %d bytes consumed by job\n",streamname,actual);
				offset+=actual;
			} else {
				dprintf(D_SYSCALLS,"StreamHandler: %s: nothing consumed by job\n",streamname);
			}
		} else if(result==0) {
			dprintf(D_SYSCALLS,"StreamHandler: %s: end of file\n",streamname);
			daemonCore->Cancel_Pipe(handler_pipe);
			daemonCore->Close_Pipe(handler_pipe);
		} else if(result<0) {
			EXCEPT("StreamHandler: %s: unable to read from %s: %s\n",streamname,filename,strerror(errno));
		}
	}

	return KEEP_STREAM;
}

int StreamHandler::GetJobPipe()
{
	return job_pipe;
}

