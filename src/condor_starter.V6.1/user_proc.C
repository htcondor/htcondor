/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
	if( JobAd ) {
		initKillSigs();
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

	const char* tmp = signalName( soft_kill_sig );
	dprintf( D_FULLDEBUG, "%s KillSignal: %d (%s)\n", 
			 name ? name : "Main job", soft_kill_sig, 
			 tmp ? tmp : "Unknown" );

	tmp = signalName( rm_kill_sig );
	dprintf( D_FULLDEBUG, "%s RmKillSignal: %d (%s)\n", 
			 name ? name : "Main job", rm_kill_sig, 
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
			proc_env->Put( env_name.GetCStr(), WTERMSIG(exit_status) );
		} else {
			env_name = base.GetCStr();
			env_name += "EXIT_CODE";
			proc_env->Put( env_name.GetCStr(), WEXITSTATUS(exit_status) );
		}
	}
}


int
UserProc::openStdFile( std_file_type type, const char* attr, 
					   bool allow_dash, bool &used_starter_fd,
					   const char* log_header, const char* phrase )
{
		// initialize this to -2 to mean "not specified".  if we have
		// success, we'll have a valid fd (>=0).  if there's an error
		// opening something, we'll return -1 which our callers
		// consider a failed open().
	int fd = -2;
	const char* filename;
	bool wants_stream = false;
	const char* name;

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
			name = "stdin";
			break;
		case SFT_OUT:
			filename = Starter->jic->jobOutputFilename();
			wants_stream = Starter->jic->streamOutput();
			name = "stdout";
			break;
		case SFT_ERR:
			filename = Starter->jic->jobErrorFilename();
			wants_stream = Starter->jic->streamError();
			name = "stderr";
			break;
		}
	}


		///////////////////////////////////////////////////////
		// Deal with the "nothing-specified" case
		///////////////////////////////////////////////////////

	if( ! filename ) {
		switch( type ) {
		case SFT_IN:
			fd = open( NULL_FILE, O_RDONLY );
			break;
		case SFT_OUT:
		case SFT_ERR:
			fd = open( NULL_FILE, O_WRONLY|O_CREAT|O_TRUNC, 0666 );
			if( fd < 0 ) {
				// if failed, try again without O_TRUNC
				fd = open( NULL_FILE, O_WRONLY|O_CREAT, 0666 );
			}
			break;
		}
		if( fd < 0 ) {
			char const *errno_str = strerror( errno );
			MyString err_msg;
			err_msg.sprintf( "Failed to open '%s' as %s: %s (errno %d)",
							 NULL_FILE, phrase, errno_str, errno );
			dprintf( D_ALWAYS, "%s\n", err_msg.Value() );
			Starter->jic->notifyStarterError( err_msg.Value(), true );
			return -1;
		}
			// if we're here, we successfully opened the NULL file, so
			// we can safely return the fd
		return fd;
	}


		///////////////////////////////////////////////////////
		// Deal with other special cases
		///////////////////////////////////////////////////////

		////////////////////////////////////
		// Use the starter's equivalent fd
		////////////////////////////////////
	if( allow_dash && filename[0] == '-' && ! filename[1] ) {
			// use the starter's fd
		used_starter_fd = true;
		switch( type ) {
		case SFT_IN:
			fd = Starter->starterStdinFd();
			dprintf( D_ALWAYS, "%s: using STDIN of %s\n", log_header,
					 mySubSystem );
			break;
		case SFT_OUT:
			fd = Starter->starterStdoutFd();
			dprintf( D_ALWAYS, "%s: using STDOUT of %s\n", log_header,
					 mySubSystem );
			break;
		case SFT_ERR:
			fd = Starter->starterStderrFd();
			dprintf( D_ALWAYS, "%s: using STDERR of %s\n", log_header,
					 mySubSystem );
			break;
		}
		return fd;
	}

		//////////////////////
		// Use streaming I/O
		//////////////////////
	if( wants_stream ) {
		StreamHandler *handler = new StreamHandler;
		if( !handler->Init(filename, name, false) ) {
			MyString err_msg;
			err_msg.sprintf( "unable to establish %s stream", phrase );
			Starter->jic->notifyStarterError( err_msg.Value(), true );
			return -1;
		}
		fd = handler->GetJobPipe();
		dprintf( D_ALWAYS, "%s: streaming from remote file %s\n",
				 log_header, filename );
		return fd;
	}


		///////////////////////////////////////////////////////
		// The regular case of a local file 
		///////////////////////////////////////////////////////

	switch( type ) {
	case SFT_IN:
		fd = open( filename, O_RDONLY );
		break;
	case SFT_OUT:
	case SFT_ERR:
		fd = open( filename, O_WRONLY|O_CREAT|O_TRUNC, 0666 );
		if( fd < 0 ) {
				// if failed, try again without O_TRUNC
			fd = open( filename, O_WRONLY|O_CREAT, 0666 );
		}
		break;
	}
	if( fd < 0 ) {
		char const *errno_str = strerror( errno );
		MyString err_msg;
		err_msg.sprintf( "Failed to open %s file '%s': %s (errno %d)",
						 phrase, filename, errno_str, errno );
		dprintf( D_ALWAYS, "%s\n", err_msg.Value() );
		Starter->jic->notifyStarterError( err_msg.Value(), true );
		return -1;
	}
	dprintf( D_ALWAYS, "%s: %s\n", log_header, filename );
	return fd;
}

