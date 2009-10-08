/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


#include "condor_common.h"
#include "stream_handler.h"
#include "NTsenders.h"

extern ReliSock *syscall_sock;

/*static*/ int StreamHandler::num_handlers = 0;

/*static*/ StreamHandler *StreamHandler::handlers[3];

StreamHandler::StreamHandler()
{    
}


bool StreamHandler::Init( const char *fn, const char *sn, bool io )
{
	int flags;
	int result;
	HandlerType handler_mode;

	filename = fn;
	streamname = sn;
	is_output = io;

	if(is_output) {
		flags = O_CREAT|O_TRUNC|O_WRONLY;
	} else {
		flags = O_RDONLY;
	}

	remote_fd = REMOTE_CONDOR_open(filename.Value(),(open_flags_t)flags,0777);
	if(remote_fd<0) {
		dprintf(D_ALWAYS,"Couldn't open %s to stream %s: %s\n",filename.Value(),streamname.Value(),strerror(errno));
		return false;
	}

	// create a DaemonCore pipe
	result = daemonCore->Create_Pipe(pipe_fds,
					 is_output,     // registerable for read if it's streamed output
					 !is_output,    // registerable for write if it's streamed input
					 false,         // blocking read
					 false,         // blocking write
					 STREAM_BUFFER_SIZE);
	if(result==0) {
		dprintf(D_ALWAYS,"Couldn't create pipe to stream %s: %s\n",streamname.Value(),strerror(errno));
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

	done = false;
	connected = true;
	handlers[num_handlers++] = this;

	return true;
}

int StreamHandler::Handler( int  /* fd */)
{
	int result;
	int actual;

	if(is_output) {
		// Output from the job to the shadow
		dprintf(D_SYSCALLS,"StreamHandler: %s: stream ready\n",streamname.Value());

		// As STREAM_BUFFER_SIZE should be <= PIPE_BUF, following should
		// never block
		result = daemonCore->Read_Pipe(handler_pipe,buffer,STREAM_BUFFER_SIZE);

		if(result>0) {
			dprintf(D_SYSCALLS,"StreamHandler: %s: %d bytes available\n",streamname.Value(),result);
			errno = 0;
			pending = result;
			REMOTE_CONDOR_lseek(remote_fd,offset,SEEK_SET);
			if (errno == ETIMEDOUT) {
				Disconnect();
				return KEEP_STREAM;
			}
	
			// This means something's really wrong with the file -- FS full
			// i/o error, so punt.

			if (errno != 0) {
				EXCEPT("StreamHandler: %s: couldn't lseek on %s to %d: %s\n",streamname.Value(),filename.Value(),(int)offset, strerror(errno));
			}

			errno = 0;
			actual = REMOTE_CONDOR_write(remote_fd,buffer,result);
			if (errno == ETIMEDOUT) {
				Disconnect();
				return KEEP_STREAM;
			}

			if(actual!=result) {
				EXCEPT("StreamHandler: %s: couldn't write to %s: %s (%d!=%d) \n",streamname.Value(),filename.Value(),strerror(errno),actual,result);
			}
			dprintf(D_SYSCALLS,"StreamHandler: %s: %d bytes written to %s\n",streamname.Value(),result,filename.Value());
			offset+=actual;
		} else if(result==0) {
			dprintf(D_SYSCALLS,"StreamHandler: %s: end of stream\n",streamname.Value());
			daemonCore->Cancel_Pipe(handler_pipe);
			daemonCore->Close_Pipe(handler_pipe);
			done=true;
			
			result = REMOTE_CONDOR_fsync(remote_fd);	

			if (result != 0) {
				// This is bad. All of our writes have succeeded, but
				// the final fsync has not.  We don't have a good way
				// to recover, as we haven't saved the data.
				// just punt

				EXCEPT("StreamHandler:: %s: couldn't fsync %s: %s\n", streamname.Value(), filename.Value(), strerror(errno));
			}

				// If close fails, that's OK, we know the bytes are on disk
			REMOTE_CONDOR_close(remote_fd);

		} else if(result<0) {
			dprintf(D_SYSCALLS,"StreamHandler: %s: unable to read: %s\n",streamname.Value(),strerror(errno));
			// Why don't we EXCEPT here?
		}
	} else {
		dprintf(D_SYSCALLS,"StreamHandler: %s: stream ready\n",streamname.Value());

		errno = 0;
		REMOTE_CONDOR_lseek(remote_fd,offset,SEEK_SET);
		if (errno == ETIMEDOUT) {
			Disconnect();
			return KEEP_STREAM;
		}

		errno = 0;
		result = REMOTE_CONDOR_read(remote_fd,buffer,STREAM_BUFFER_SIZE);
		if (errno == ETIMEDOUT) {
			Disconnect();
			return KEEP_STREAM;
		}

		if(result>0) {
			dprintf(D_SYSCALLS,"StreamHandler: %s: %d bytes read from %s\n",streamname.Value(),result,filename.Value());
			actual = daemonCore->Write_Pipe(handler_pipe,buffer,result);
			if(actual>=0) {
				dprintf(D_SYSCALLS,"StreamHandler: %s: %d bytes consumed by job\n",streamname.Value(),actual);
				offset+=actual;
			} else {
				dprintf(D_SYSCALLS,"StreamHandler: %s: nothing consumed by job\n",streamname.Value());
			}
		} else if(result==0) {
			dprintf(D_SYSCALLS,"StreamHandler: %s: end of file\n",streamname.Value());
			done=true;
			daemonCore->Cancel_Pipe(handler_pipe);
			daemonCore->Close_Pipe(handler_pipe);
			REMOTE_CONDOR_close(remote_fd);
		} else if(result<0) {
			EXCEPT("StreamHandler: %s: unable to read from %s: %s\n",streamname.Value(),filename.Value(),strerror(errno));
		}
	}

	return KEEP_STREAM;
}

int StreamHandler::GetJobPipe()
{
	return job_pipe;
}

/*static*/ int
StreamHandler::ReconnectAll() {
	for (int i = 0; i < num_handlers; i++) {
		if (!handlers[i]->done) {
			if (handlers[i]->connected) {
				handlers[i]->Disconnect();
			}
			handlers[i]->Reconnect();
		}
	}
	return 0;
}

bool
StreamHandler::Reconnect() {
	int flags;
	HandlerType handler_mode;

	dprintf(D_ALWAYS, "Streaming i/o handler reconnecting %s to shadow\n", filename.Value());

	if(is_output) {
		flags = O_WRONLY;
		handler_mode = HANDLE_READ;
	} else {
		flags = O_RDONLY;
		handler_mode = HANDLE_WRITE;
	}

	remote_fd = REMOTE_CONDOR_open(filename.Value(),(open_flags_t)flags,0777);
	if(remote_fd<0) {
		EXCEPT("Couldn't reopen %s to stream %s: %s\n",filename.Value(),streamname.Value(),strerror(errno));
	}

	daemonCore->Register_Pipe(handler_pipe,"Job I/O Pipe",(PipeHandlercpp)&StreamHandler::Handler,"Stream I/O Handler",this,handler_mode);
	
	connected = true;

	if(is_output) {
		
		VerifyOutputFile();

		// We don't always need to do this write, but it is cheap to do
		// and always doing it makes it easier to test

		errno = 0;
		dprintf(D_ALWAYS, "Retrying streaming write to %s of %d bytes after reconnect\n", filename.Value(), pending);
		REMOTE_CONDOR_lseek(remote_fd,offset,SEEK_SET);
		if (errno == ETIMEDOUT) {
			Disconnect();
			return false;
		}
		errno = 0;
		int actual = REMOTE_CONDOR_write(remote_fd,buffer,pending);
		if (errno == ETIMEDOUT) {
			Disconnect();
			return 0;
			return false;
		}
		if(actual!=pending) {
			EXCEPT("StreamHandler: %s: couldn't write to %s: %s (%d!=%d) \n",streamname.Value(),filename.Value(),strerror(errno),actual,pending);
		}
		dprintf(D_SYSCALLS,"StreamHandler: %s: %d bytes written to %s\n",streamname.Value(),pending,filename.Value());
		offset+=actual;
	} else {

			// In the input case, we never wrote to the job pipe,
			// and reading is idempotent, so just try again.
		this->Handler(0);
	}

	return true;
}

void
StreamHandler::Disconnect() {

	dprintf(D_ALWAYS, "Streaming i/o handler disconnecting %s from shadow\n", filename.Value());
	connected=false;
	daemonCore->Cancel_Pipe(handler_pipe);
	syscall_sock->close();
}

// On reconnect, the submit OS may have crashed and lost our precious
// output bytes.  We only save the last buffer's worth of data
// check the current size of the file.  If it is complete, or within a
// buffer's length, we're OK.  Otherwise, except
bool
StreamHandler::VerifyOutputFile() {
	errno = 0;
	off_t size = REMOTE_CONDOR_lseek(remote_fd,0,SEEK_END);
	
	if (size == (off_t)-1) {
		EXCEPT("StreamHandler: cannot lseek to output file %s on reconnect: %d\n", filename.Value(), errno);
		return false;
	}

	if (size < offset) {
		// Damn.  Older writes that returned successfully didn't actually
		// survive the reboot (or maybe someone else mucked with the file
		// in the interim.  Rerun the whole job
		EXCEPT("StreamHandler: output file %s is length %d, expected at least %d\n", filename.Value(), (int)size, (int)offset);
		return false;
	}
	return true;
}
