/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "condor_classad.h"
#include "user_proc.h"
#include "starter.h"
#include "condor_attributes.h"
#include "classad_helpers.h"
#include "sig_name.h"
#include "stream_handler.h"
#include "starter_privsep_helper.h"

extern CStarter *Starter;


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
	job_universe = 0;  // we'll fill in a real value if we can...
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
UserProc::PublishUpdateAd( ClassAd* ad )
{
	char buf[256];

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

	if( exit_status >= 0 ) {

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
	if( exit_status >= 0 ) {
			// TODO: what should these really be called?  use
			// myDistro?  mySubSystem?  hard to say...
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
			env_name = base.GetCStr();
			env_name += "EXIT_SIGNAL";
			proc_env->SetEnv( env_name.GetCStr(), WTERMSIG(exit_status) );
		} else {
			env_name = base.GetCStr();
			env_name += "EXIT_CODE";
			proc_env->SetEnv( env_name.GetCStr(), WEXITSTATUS(exit_status) );
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

	const char* filename;
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
	}

		///////////////////////////////////////////////////////
		// Figure out what we're trying to open, if anything
		///////////////////////////////////////////////////////

	if( attr ) {
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

	if( ! filename ) {
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
			err_msg.sprintf( "unable to establish %s stream", phrase );
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
	if( allow_dash && (filename[0] == '-') && !filename[1] )
	{
			// use the starter's fd
		switch( type ) {
		case SFT_IN:
			*out_fd = Starter->starterStdinFd();
			dprintf( D_ALWAYS, "%s: using STDIN of %s\n", log_header,
					 mySubSystem );
			break;
		case SFT_OUT:
			*out_fd = Starter->starterStdoutFd();
			dprintf( D_ALWAYS, "%s: using STDOUT of %s\n", log_header,
					 mySubSystem );
			break;
		case SFT_ERR:
			*out_fd = Starter->starterStderrFd();
			dprintf( D_ALWAYS, "%s: using STDERR of %s\n", log_header,
					 mySubSystem );
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
		int flags = O_WRONLY | O_CREAT | O_TRUNC | O_APPEND;
		fd = safe_open_wrapper( filename.Value(), flags, 0666 );
		if( fd < 0 ) {
				// if failed, try again without O_TRUNC
			flags &= ~O_TRUNC;
			fd = safe_open_wrapper( filename.Value(), flags, 0666 );
		}
	} else {
		fd = safe_open_wrapper( filename.Value(), O_RDONLY );
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
		err_msg.sprintf( "Failed to open '%s' as %s: %s (errno %d)",
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

