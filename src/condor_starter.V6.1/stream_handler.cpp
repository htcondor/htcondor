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
#include "starter.h"
#include "jic_shadow.h"

extern Starter *Starter;

/* static */ std::list< StreamHandler * > StreamHandler::handlers;

StreamHandler::StreamHandler() : is_output(false), job_pipe(-1), handler_pipe(-1), remote_fd(-1), offset(0), flags(0), done(false), connected(0), pending(0)
{
	pipe_fds[0] = -1; pipe_fds[1] = -1;
	buffer[0] = '\0';
}


bool StreamHandler::Init( const char *fn, const char *sn, bool io, int f )
{
	int result;
	HandlerType handler_mode;

	filename = fn;
	streamname = sn;
	is_output = io;

	flags = f;
	if( flags == -1 ) {
		if( is_output ) {
			flags = O_CREAT | O_TRUNC | O_WRONLY;
		} else {
			flags = O_RDONLY;
		}
	}

	bool shouldAppend = false;
	if( flags & O_APPEND ) {
		// If we actually pass O_APPEND, then when we reconnect, flushing the
		// buffer will result in duplicate data if the far side suffered a
		// partial write.  (If it failed to write the buffer at all, that's
		// OK, because we /wanted/ to append it to the file.)
		//
		// Instead, unset O_APPEND and simulate the effect: seek to the end of
		// the file on open and set offset to that.
		shouldAppend = true;
		flags = flags & (~O_APPEND);
	}

	remote_fd = REMOTE_CONDOR_open(filename.c_str(),(open_flags_t)flags,0666);
	if(remote_fd<0) {
		dprintf(D_ALWAYS,"Couldn't open %s to stream %s: %s\n",filename.c_str(),streamname.c_str(),strerror(errno));
		return false;
	}

	offset = 0;
	if( shouldAppend ) {
		offset = REMOTE_CONDOR_lseek( remote_fd, 0, SEEK_END );
		if( offset < 0 ) {
			dprintf( D_ALWAYS, "Couldn't seek to end of %s for stream %s when append mode was requested: %s\n", filename.c_str(), streamname.c_str(), strerror( errno ) );
			return false;
		}
		dprintf( D_SYSCALLS, "StreamHandler: Sought to %ld as a result of append request.\n", (long)offset );
	}

	// create a DaemonCore pipe
	result = daemonCore->Create_Pipe(pipe_fds,
					 is_output,     // registerable for read if it's streamed output
					 !is_output,    // registerable for write if it's streamed input
					 false,         // blocking read
					 false,         // blocking write
					 STREAM_BUFFER_SIZE);
	if(result==0) {
		dprintf(D_ALWAYS,"Couldn't create pipe to stream %s: %s\n",streamname.c_str(),strerror(errno));
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

	daemonCore->Register_Pipe(handler_pipe,"Job I/O Pipe",static_cast<PipeHandlercpp>(&StreamHandler::Handler),"Stream I/O Handler",this,handler_mode);

	done = false;
	connected = true;
	handlers.push_back( this );

	return true;
}

extern ReliSock * syscall_sock;
int StreamHandler::Handler( int  /* fd */)
{
	int result;
	int actual;

	//
	// If we're disconnected from the shadow, the REMOTE_CONDOR_* calls in
	// this function will ASSERT() because of the bogus syscall_sock.  This
	// is a problem if we're waiting for the shadow to reconnect.
	//
	if( syscall_sock && syscall_sock->get_file_desc() == INVALID_SOCKET ) {
		Disconnect();

		// If we're writing, empty the pipe and check the write on reconnect.
		// The Disconnect() above unregisters this handler, so the application
		// should block until then, and we don't have to worry about losing
		// any more data.
		result = daemonCore->Read_Pipe( handler_pipe, buffer, STREAM_BUFFER_SIZE );
		if( result > 0 ) {
			pending = result;
		} else if( result == 0 ) {
			// We know the remote fsync will fail, but it might make sense
			// to try it again after the reconnect, so we do nothing.
			// (The handler has already been unregistered; when we reregister,
			// it should show up as ready to read again, and return EOF again.)
		} else {
			// See question below.
		}

		return KEEP_STREAM;
	}

	if(is_output) {
		// Output from the job to the shadow
		dprintf(D_SYSCALLS,"StreamHandler: %s: stream ready\n",streamname.c_str());

		// As STREAM_BUFFER_SIZE should be <= PIPE_BUF, following should
		// never block
		result = daemonCore->Read_Pipe(handler_pipe,buffer,STREAM_BUFFER_SIZE);

		if(result>0) {
			dprintf(D_SYSCALLS,"StreamHandler: %s: %d bytes available, seeking to offset %ld\n",streamname.c_str(),result,(long)offset);
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
				EXCEPT("StreamHandler: %s: couldn't lseek on %s to %d: %s",streamname.c_str(),filename.c_str(),(int)offset, strerror(errno));
			}

			errno = 0;
			actual = REMOTE_CONDOR_write(remote_fd,buffer,result);
			if (errno == ETIMEDOUT) {
				Disconnect();
				return KEEP_STREAM;
			}

			if(actual!=result) {
				EXCEPT("StreamHandler: %s: couldn't write to %s: %s (%d!=%d)",streamname.c_str(),filename.c_str(),strerror(errno),actual,result);
			}
			dprintf(D_SYSCALLS,"StreamHandler: %s: %d bytes written to %s\n",streamname.c_str(),result,filename.c_str());
			offset+=actual;
		} else if(result==0) {
			dprintf(D_SYSCALLS,"StreamHandler: %s: end of stream\n",streamname.c_str());
			daemonCore->Cancel_Pipe(handler_pipe);
			daemonCore->Close_Pipe(handler_pipe);
			done=true;

			result = REMOTE_CONDOR_fsync(remote_fd);

			if (result != 0) {
				// This is bad. All of our writes have succeeded, but
				// the final fsync has not.  We don't have a good way
				// to recover, as we haven't saved the data.
				// just punt

				EXCEPT("StreamHandler:: %s: couldn't fsync %s: %s", streamname.c_str(), filename.c_str(), strerror(errno));
			}

				// If close fails, that's OK, we know the bytes are on disk
			REMOTE_CONDOR_close(remote_fd);

			handlers.remove( this );
		} else if(result<0) {
			dprintf(D_SYSCALLS,"StreamHandler: %s: unable to read: %s\n",streamname.c_str(),strerror(errno));
			// Why don't we EXCEPT here?
		}
	} else {
		dprintf(D_SYSCALLS,"StreamHandler: %s: stream ready\n",streamname.c_str());

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
			dprintf(D_SYSCALLS,"StreamHandler: %s: %d bytes read from %s\n",streamname.c_str(),result,filename.c_str());
			actual = daemonCore->Write_Pipe(handler_pipe,buffer,result);
			if(actual>=0) {
				dprintf(D_SYSCALLS,"StreamHandler: %s: %d bytes consumed by job\n",streamname.c_str(),actual);
				offset+=actual;
			} else {
				dprintf(D_SYSCALLS,"StreamHandler: %s: nothing consumed by job\n",streamname.c_str());
			}
		} else if(result==0) {
			dprintf(D_SYSCALLS,"StreamHandler: %s: end of file\n",streamname.c_str());
			done=true;
			daemonCore->Cancel_Pipe(handler_pipe);
			daemonCore->Close_Pipe(handler_pipe);
			REMOTE_CONDOR_close(remote_fd);
			handlers.remove( this );
		} else if(result<0) {
			EXCEPT("StreamHandler: %s: unable to read from %s: %s",streamname.c_str(),filename.c_str(),strerror(errno));
		}
	}

	return KEEP_STREAM;
}

int StreamHandler::GetJobPipe() const
{
	return job_pipe;
}

/* static */ int
StreamHandler::ReconnectAll() {
	std::list< StreamHandler * >::iterator i = handlers.begin();
	for( ; i != handlers.end(); ++i ) {
		StreamHandler * handler = * i;
		if( ! handler->done ) {
			if( handler->connected ) {
				// Do NOT call Disconnect().  This reconnect only happens
				// AFTER the syscall sock has been reestablished, so we
				// very much do not want to close the syscall sock again.
				handler->connected = false;
				daemonCore->Cancel_Pipe( handler->handler_pipe );
			}

			handler->Reconnect();
		}
	}

	return 0;
}

bool
StreamHandler::Reconnect() {
	HandlerType handler_mode;

	dprintf(D_ALWAYS, "Streaming i/o handler reconnecting %s to shadow\n", filename.c_str());

	if(is_output) {
		handler_mode = HANDLE_READ;
	} else {
		handler_mode = HANDLE_WRITE;
	}

	// Never permit truncation on a reconnect; what sense does that make?
	flags = flags & (~O_TRUNC);

	remote_fd = REMOTE_CONDOR_open(filename.c_str(),(open_flags_t)flags,0666);
	if(remote_fd<0) {
		EXCEPT("Couldn't reopen %s to stream %s: %s",filename.c_str(),streamname.c_str(),strerror(errno));
	}

	daemonCore->Register_Pipe(handler_pipe,"Job I/O Pipe",static_cast<PipeHandlercpp>(&StreamHandler::Handler),"Stream I/O Handler",this,handler_mode);

	connected = true;

	if(is_output) {

		VerifyOutputFile();

		// We don't always need to do this write, but it is cheap to do
		// and always doing it makes it easier to test

		errno = 0;
		dprintf(D_ALWAYS, "Retrying streaming write to %s of %d bytes at %ld after reconnect\n", filename.c_str(), pending, (long)offset);
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
			EXCEPT("StreamHandler: %s: couldn't write to %s: %s (%d!=%d)",streamname.c_str(),filename.c_str(),strerror(errno),actual,pending);
		}
		dprintf(D_SYSCALLS,"StreamHandler: %s: %d bytes written to %s\n",streamname.c_str(),pending,filename.c_str());
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
	dprintf(D_ALWAYS, "Streaming i/o handler disconnecting %s from shadow\n", filename.c_str());
	connected=false;
	daemonCore->Cancel_Pipe(handler_pipe);
	Starter->jic->disconnect();
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
		EXCEPT("StreamHandler: cannot lseek to output file %s on reconnect: %d", filename.c_str(), errno);
		return false;
	}

	if (size < offset) {
		// Damn.  Older writes that returned successfully didn't actually
		// survive the reboot (or maybe someone else mucked with the file
		// in the interim.  Rerun the whole job
		EXCEPT("StreamHandler: output file %s is length %d, expected at least %d", filename.c_str(), (int)size, (int)offset);
		return false;
	}
	return true;
}
