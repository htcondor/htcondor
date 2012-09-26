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
#include "condor_classad.h"
#include "user_proc.h"
#include "starter.h"
#include "condor_attributes.h"
#include "classad_helpers.h"
#include "sig_name.h"
#include "stream_handler.h"
#include "subsystem_info.h"

extern CStarter *Starter;

const char* JOB_WRAPPER_FAILURE_FILE = ".job_wrapper_failure";

/* UserProc class implementation */

UserProc::~UserProc()
{
	if( name ) {
		free( name );
	}
}


void
UserProc::initialize( void )
{
	JobPid = -1;
	exit_status = -1;
	requested_exit = false;
	m_proc_exited = false;
	m_dedicated_account = NULL;
	job_universe = 0;  // we'll fill in a real value if we can...
	int i;
	for(i=0;i<3;i++) {
		m_pre_defined_std_fds[i] = -1;
		m_pre_defined_std_fnames[i] = NULL;
	}
	if( JobAd ) {
		initKillSigs();
		if( JobAd->LookupInteger( ATTR_JOB_UNIVERSE, job_universe ) < 1 ) {
			job_universe = 0;
		}
	}
}


void
UserProc::initKillSigs( void )
{
	int sig;

	sig = findSoftKillSig( JobAd );
	if( sig >= 0 ) {
		soft_kill_sig = sig;
	} else {
		soft_kill_sig = SIGTERM;
	}

	sig = findRmKillSig( JobAd );
	if( sig >= 0 ) {
		rm_kill_sig = sig;
	} else {
		rm_kill_sig = SIGTERM;
	}

	sig = findHoldKillSig( JobAd );
	if( sig >= 0 ) {
		hold_kill_sig = sig;
	} else {
		hold_kill_sig = SIGTERM;
	}

	const char* tmp = signalName( soft_kill_sig );
	dprintf( D_FULLDEBUG, "%s KillSignal: %d (%s)\n", 
			 name ? name : "Main job", soft_kill_sig, 
			 tmp ? tmp : "Unknown" );

	tmp = signalName( rm_kill_sig );
	dprintf( D_FULLDEBUG, "%s RmKillSignal: %d (%s)\n", 
			 name ? name : "Main job", rm_kill_sig, 
			 tmp ? tmp : "Unknown" );

	tmp = signalName( hold_kill_sig );
	dprintf( D_FULLDEBUG, "%s HoldKillSignal: %d (%s)\n", 
			 name ? name : "Main job", hold_kill_sig, 
			 tmp ? tmp : "Unknown" );
}


bool
UserProc::JobReaper(int pid, int status)
{
	MyString line;
	MyString error_txt;
	MyString filename;
	const char* dir = Starter->GetWorkingDir();
	FILE* fp;

	dprintf( D_FULLDEBUG, "Inside UserProc::JobReaper()\n" );

	filename.formatstr("%s%c%s", dir, DIR_DELIM_CHAR, JOB_WRAPPER_FAILURE_FILE);
	if (0 == access(filename.Value(), F_OK)) {
		// The job wrapper failed, so read the contents of the file
		// and EXCEPT, just as is done when an executable is unable
		// to be run.  Ideally, both failure cases would propagate
		// into the job ad
		fp = safe_fopen_wrapper_follow(filename.Value(), "r");
		if (!fp) {
			dprintf(D_ALWAYS, "Unable to open \"%s\" for reading: "
					"%s (errno %d)\n", filename.Value(),
					strerror(errno), errno);
		} else {
			while (line.readLine(fp))
			{
				error_txt += line;
			}
			fclose(fp);
		}
		error_txt.trim();
		EXCEPT("The job wrapper failed to execute the job: %s", error_txt.Value());
	}

	if (JobPid == pid) {
		m_proc_exited = true;
		exit_status = status;
		job_exit_time.getTime();
	}
	return m_proc_exited;
}


bool
UserProc::PublishUpdateAd( ClassAd* ad )
{
	char buf[256];

	dprintf( D_FULLDEBUG, "Inside UserProc::PublishUpdateAd()\n" );

	if( JobPid >= 0 ) { 
		sprintf( buf, "%s%s=%d", name ? name : "", ATTR_JOB_PID,
				 JobPid );
		ad->Insert( buf );
	}

	if( job_start_time.seconds() > 0 ) {
		sprintf( buf, "%s%s=%ld", name ? name : "", ATTR_JOB_START_DATE,
				 job_start_time.seconds() );
		ad->Insert( buf );
	}

	if (m_proc_exited) {

		if( job_exit_time.seconds() > 0 ) {
			sprintf( buf, "%s%s=%f", name ? name : "", ATTR_JOB_DURATION, 
					 job_exit_time.difference(&job_start_time) );
			ad->Insert( buf );
		}

			/*
			  If we have the exit status, we want to parse it and set
			  some attributes which describe the status in a platform
			  independent way.  This way, we're sure we're analyzing
			  the status integer with the platform-specific macros
			  where it came from, instead of assuming that WIFEXITED()
			  and friends will work correctly on a status integer we
			  got back from a different platform.
			*/
		if( WIFSIGNALED(exit_status) ) {
			sprintf( buf, "%s%s = TRUE", name ? name : "",
					 ATTR_ON_EXIT_BY_SIGNAL );
			ad->Insert( buf );
			sprintf( buf, "%s%s = %d", name ? name : "",
					 ATTR_ON_EXIT_SIGNAL, WTERMSIG(exit_status) );
			ad->Insert( buf );
			sprintf( buf, "%s%s = \"died on %s\"",
					 name ? name : "", ATTR_EXIT_REASON,
					 daemonCore->GetExceptionString(WTERMSIG(exit_status)) );
			ad->Insert( buf );
		} else {
			sprintf( buf, "%s%s = FALSE", name ? name : "",
					 ATTR_ON_EXIT_BY_SIGNAL );
			ad->Insert( buf );
			sprintf( buf, "%s%s = %d", name ? name : "",
					 ATTR_ON_EXIT_CODE, WEXITSTATUS(exit_status) );
			ad->Insert( buf );
		}
	}
	return true;
}


void
UserProc::PublishToEnv( Env* proc_env )
{
	if (m_proc_exited) {
			// TODO: what should these really be called?  use
			// myDistro?  get_mySubSystem()?  hard to say...
		MyString base;
		base = "_";
		base += myDistro->Get();
		base += '_';
		if( name ) {
			base += name;
		} else {
			base += "MAINJOB";
		}
		base += '_';
		base.upper_case();
 
		MyString env_name;

		if( WIFSIGNALED(exit_status) ) {
			env_name = base.Value();
			env_name += "EXIT_SIGNAL";
			proc_env->SetEnv( env_name.Value(), WTERMSIG(exit_status) );
		} else {
			env_name = base.Value();
			env_name += "EXIT_CODE";
			proc_env->SetEnv( env_name.Value(), WEXITSTATUS(exit_status) );
		}
	}
}

bool
UserProc::getStdFile( std_file_type type,
                      const char* attr,
                      bool allow_dash,
                      const char* log_header,
                      int* out_fd,
                      MyString* out_name)
{
		// we're going to return true on success, false on error.
		// if we succeed, then if we have an open FD that should
		// be passed down to the job, return it in the descriptor
		// argument. otherwise, return -1 in the descriptor
		// argument and return the name of the file that should
		// be opened for the child in the name argument
	ASSERT(out_fd != NULL);
	ASSERT(out_name != NULL);
	*out_fd = -1;

	const char* filename = NULL;
	bool wants_stream = false;
	const char* stream_name = NULL;
	const char* phrase = NULL;
	bool is_output = true;
	bool is_null_file = false;

		///////////////////////////////////////////////////////
		// Initialize some settings depending on the type
		///////////////////////////////////////////////////////

	switch( type ) {
	case SFT_IN:
		is_output = false;
		phrase = "standard input";
		stream_name = "stdin";
		break;
	case SFT_OUT:
		is_output = true;
		phrase = "standard output";
		stream_name = "stdout";
		break;
	case SFT_ERR:
		is_output = true;
		phrase = "standard error";
		stream_name = "stderr";
		break;
	default:
		EXCEPT("Programmer error in getStdFile, type =%d", type);
	}

		///////////////////////////////////////////////////////
		// Figure out what we're trying to open, if anything
		///////////////////////////////////////////////////////

	if( m_pre_defined_std_fds[type] != -1 ||
		m_pre_defined_std_fnames[type] != NULL )
	{
		filename = m_pre_defined_std_fnames[type];
		*out_fd = m_pre_defined_std_fds[type];
		if( *out_fd != -1 ) {
				// we must dup() the file descriptor because our caller
				// will take ownership of it (i.e. they should close it)
			*out_fd = dup(*out_fd);
			if( *out_fd == -1 ) {
				dprintf(D_ALWAYS,"Failed to dup fd %d for %s\n",
						m_pre_defined_std_fds[type],phrase);
				return false;
			}
		}
		wants_stream = false;
	} else if( attr ) {
		filename = Starter->jic->getJobStdFile( attr );
		wants_stream = Starter->jic->streamStdFile( attr );
	} else {
		switch( type ) {
		case SFT_IN:
			filename = Starter->jic->jobInputFilename();
			wants_stream = Starter->jic->streamInput();
			break;
		case SFT_OUT:
			filename = Starter->jic->jobOutputFilename();
			wants_stream = Starter->jic->streamOutput();
			break;
		case SFT_ERR:
			filename = Starter->jic->jobErrorFilename();
			wants_stream = Starter->jic->streamError();
			break;
		}
	}

	if( ! filename && *out_fd == -1) {
			// If there's nothing specified, we always want to open
			// the system-appropriate NULL file (/dev/null or NUL).
			// Otherwise, we can mostly treat this as if the job
			// defined a real local filename.  For the few cases were
			// we have to behave differently, record it in a bool. 
		filename = NULL_FILE;
		is_null_file = true;
	}

		///////////////////////////////////////////////////////
		// Deal with special cases
		///////////////////////////////////////////////////////

		//////////////////////
		// Use streaming I/O
		//////////////////////
	if( wants_stream && ! is_null_file ) {
		StreamHandler *handler = new StreamHandler;
		if( !handler->Init(filename, stream_name, is_output) ) {
			MyString err_msg;
			err_msg.formatstr( "unable to establish %s stream", phrase );
			Starter->jic->notifyStarterError( err_msg.Value(), true,
			    is_output ? CONDOR_HOLD_CODE_UnableToOpenOutputStream :
			                CONDOR_HOLD_CODE_UnableToOpenInputStream, 0 );
			return false;
		}
		*out_fd = handler->GetJobPipe();
		dprintf( D_ALWAYS, "%s: streaming from remote file %s\n",
				 log_header, filename );
		return true;
	}

		////////////////////////////////////
		// Use the starter's equivalent fd
		////////////////////////////////////
	if( allow_dash && filename && (filename[0] == '-') && !filename[1] )
	{
			// use the starter's fd
		switch( type ) {
		case SFT_IN:
			*out_fd = Starter->starterStdinFd();
			dprintf( D_ALWAYS, "%s: using STDIN of %s\n", log_header,
					 get_mySubSystem()->getName() );
			break;
		case SFT_OUT:
			*out_fd = Starter->starterStdoutFd();
			dprintf( D_ALWAYS, "%s: using STDOUT of %s\n", log_header,
					 get_mySubSystem()->getName() );
			break;
		case SFT_ERR:
			*out_fd = Starter->starterStderrFd();
			dprintf( D_ALWAYS, "%s: using STDERR of %s\n", log_header,
					 get_mySubSystem()->getName() );
			break;
		}
		return true;
	}

		///////////////////////////////////////////////////////
		// The regular case of a local file 
		///////////////////////////////////////////////////////
	*out_name = filename;
	return true;
}

int
UserProc::openStdFile( std_file_type type,
                       const char* attr, 
                       bool allow_dash,
                       const char* log_header)
{
		// most of the work gets done by our helper, getStdFile
	int fd;
	MyString filename;
	if (!getStdFile(type,
	                attr,
	                allow_dash,
	                log_header,
	                &fd,
	                &filename))
	{
		return -1;
	}

		// if getStdFile came back with an FD, just return it
	if (fd != -1) {
		return fd;
	}

		// otherwise, we need to perform an open on the name
		// we got back
	bool is_output = (type != SFT_IN);
	if( is_output ) {
		int flags = O_WRONLY | O_CREAT | O_TRUNC | O_APPEND | O_LARGEFILE;
		fd = safe_open_wrapper_follow( filename.Value(), flags, 0666 );
		if( fd < 0 ) {
				// if failed, try again without O_TRUNC
			flags &= ~O_TRUNC;
			fd = safe_open_wrapper_follow( filename.Value(), flags, 0666 );
		}
	} else {
		fd = safe_open_wrapper_follow( filename.Value(), O_RDONLY | O_LARGEFILE );
	}
	if( fd < 0 ) {
		int open_errno = errno;
		char const *errno_str = strerror( errno );
		MyString err_msg;
		const char* phrase;
		if (type == SFT_IN) {
			phrase = "standard input";
		}
		else if (type == SFT_OUT) {
			phrase = "standard output";
		}
		else {
			phrase = "standard error";
		}
		err_msg.formatstr( "Failed to open '%s' as %s: %s (errno %d)",
		                 filename.Value(),
		                 phrase,
		                 errno_str,
		                 errno );
		dprintf( D_ALWAYS, "%s\n", err_msg.Value() );
		Starter->jic->notifyStarterError( err_msg.Value(), true,
		  is_output ? CONDOR_HOLD_CODE_UnableToOpenOutput :
		              CONDOR_HOLD_CODE_UnableToOpenInput, open_errno );
		return -1;
	}
	dprintf( (filename == NULL_FILE) ? D_FULLDEBUG : D_ALWAYS,
	         "%s: %s\n", log_header,
	         filename.Value() );
	return fd;
}

void
UserProc::SetStdFiles(int std_fds[], char const *std_fnames[])
{
		// store the pre-defined std files for use by getStdFile()
	int i;
	for(i=0;i<3;i++) {
		m_pre_defined_std_fds[i] = std_fds[i];
		m_pre_defined_std_fname_buf[i] = std_fnames[i];
		m_pre_defined_std_fnames[i] = std_fnames[i] ? m_pre_defined_std_fname_buf[i].Value() : NULL;
	}
}

bool
UserProc::ThisProcRunsAlongsideMainProc()
{
	return false;
}

char const *
UserProc::getArgv0()
{
	return CONDOR_EXEC;
}
