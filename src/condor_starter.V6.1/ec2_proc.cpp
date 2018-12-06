/***************************************************************
 *
 * Copyright (C) 2018, Condor Team, Computer Sciences Department,
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
#include "condor_debug.h"
#include "condor_config.h"

#include "os_proc.h"
#include "ec2_proc.h"

EC2Proc::EC2Proc( ClassAd * jobAd ) : OsProc( jobAd ),
  jobAdPipeID(-1), updateAdPipeID(-1), stderrPipeID(-1) {
	dprintf( D_PERF_TRACE, "EC2Proc::EC2Proc( %p )\n", JobAd );
}

EC2Proc::~EC2Proc() {
	dprintf( D_PERF_TRACE, "EC2Proc::~EC2Proc()\n" );
}

int
EC2Proc::StartJob() {
	dprintf( D_PERF_TRACE, "EC2Proc::StartJob()\n" );

	//
	// Start the grid manager in 'starter' mode.
	//

	int jobAdPipeFDs[2];
	if(! daemonCore->Create_Pipe( jobAdPipeFDs, false, false, true, true )) {
		dprintf( D_ALWAYS, "EC2Proc::StartJob(): failed to create job ad pipe.\n" );
		return false;
	}

	int updateAdPipeFDs[2];
	if(! daemonCore->Create_Pipe( updateAdPipeFDs, false, false, true, true )) {
		dprintf( D_ALWAYS, "EC2Proc::StartJob(): failed to create update ad pipe.\n" );
		return false;
	}

	int stderrPipeFDs[2];
	if(! daemonCore->Create_Pipe( stderrPipeFDs, false, false, true, true )) {
		dprintf( D_ALWAYS, "EC2Proc::StartJob(): failed to create stderr pipe.\n" );
		return false;
	}

	int fds[] = { jobAdPipeFDs[0], updateAdPipeFDs[1], stderrPipeFDs[1] };

	ArgList args;
	args.AppendArg( "condor_gridmanager" );
	args.AppendArg( "-f" );
	args.AppendArg( "-t" );
	args.AppendArg( "-B" );
	args.AppendArg( "-S" );
	// FIXME: for Windows.
	args.AppendArg( "/tmp" );

	// I don't know what the lifetime of this object is or who's responsible
	// for it, but I can't kill the child process [family] without it.
	FamilyInfo * fi = new FamilyInfo();

	JobPid = daemonCore->Create_Process(
		param( "GRIDMANAGER" ) /* does this leak? */,
		args, PRIV_USER_FINAL, 1, FALSE, FALSE, NULL, NULL, fi, NULL, fds );
	if( JobPid == FALSE ) {
		// See OsProc::StartJob() for better error-handling.
		JobPid = -1;
		return false;
	} else {
		dprintf( D_PERF_TRACE, "EC2Proc::StartJob(): condor_gridmanager started with PID %d.\n", JobPid );
	}

	for( int i = 0; i < 3; ++i ) {
		daemonCore->Close_FD( fds[i] );
	}

	this->jobAdPipeID = jobAdPipeFDs[1];
	this->updateAdPipeID = updateAdPipeFDs[0];
	this->stderrPipeID = stderrPipeFDs[0];

	//
	// Write the job ad to the grid manager.  If at some point we ever care,
	// we can set a daemon core timer to write the surplus byte(s).  (Since
	// we have to handle short writes anyway, we may as well start off with
	// nonblocking pipes.)
	//
	// The copy may not be necessary, but it seems safer.
	//
	std::string jobAdStr;
	ClassAd modifiedJobAd( * JobAd );
	modifiedJobAd.InsertAttr( ATTR_JOB_UNIVERSE, 9 );
	sPrintAd( jobAdStr, modifiedJobAd );
	size_t len = jobAdStr.length();
	const char * buf = jobAdStr.c_str();
	size_t bytesSoFar = 0;
	for( int i = 0; i < 20 /* seconds of careful research */; ++i ) {
		dprintf( D_PERF_TRACE, "EC2Proc::StartJob(): calling Write_Pipe( %d, %p, %lu )...\n", this->jobAdPipeID, buf, len );
		int rv = daemonCore->Write_Pipe( this->jobAdPipeID, buf, len );
		dprintf( D_PERF_TRACE, "EC2Proc::StartJob(): ... rv = %d\n", rv );
		if( rv == -1 ) {
			dprintf( D_ALWAYS, "EC2Proc::StartJob(): error writing job ad to grid manager (%d): '%s'.\n", errno, strerror(errno) );
			break;
		} else {
			bytesSoFar += rv;
			len -= rv;
			buf += rv;
			dprintf( D_PERF_TRACE, "EC2Proc::StartJob(): wrote %lu bytes out of %lu, %lu remaining.\n", bytesSoFar, jobAdStr.length(), len );
			if( bytesSoFar != jobAdStr.length() ) {
				sleep(1);
			} else {
				break;
			}
		}
	}
	if( bytesSoFar != jobAdStr.length() ) {
		dprintf( D_ALWAYS, "EC2Proc::StartJob(): failed to write job ad to grid manager.\n" );
		daemonCore->Kill_Family( JobPid );
		JobPid = -1;
		return false;
	}

	return true;
}

bool
EC2Proc::JobReaper( int pid, int status ) {
	dprintf( D_PERF_TRACE, "EC2Proc::JobReaper( %d, %d )\n", pid, status );
	if( pid != JobPid ) {
		return false;
	}

	//
	// FIXME: Check for a last job update.
	//

	//
	// FIXME: Check to see if this was a voluntary termination.
	//

	//
	// Check stderr for problems.
	//
	char buf[4097];
	int rv = daemonCore->Read_Pipe( this->stderrPipeID, buf, 4096 );
	if( rv == -1 ) {
		dprintf( D_PERF_TRACE, "EC2Proc::JobReaper( %d, %d ): error reading stderr pipe (%d): '%s'.\n", pid, status, errno, strerror(errno) );
		return true;
	}
	buf[rv] = '\0';
	if( rv != 0 ) {
		dprintf( D_ALWAYS, "EC2Proc::JobReaper( %d, %d ): grid manager's stderr was '%s'.\n", pid, status, buf );
	}

	//
	// FIXME: Close the pipes.
	//

	JobPid = -1;
	return true;
}

bool
EC2Proc::JobExit() {
	dprintf( D_PERF_TRACE, "EC2Proc::JobExit()\n" );
	return OsProc::JobExit();
}

bool
EC2Proc::PublishUpdateAd( ClassAd * ad ) {
	// Don't publish updates for now.  At some point, this is where we'd
	// dump the captive grid manager's standard out.
	dprintf( D_PERF_TRACE, "EC2Proc::PublishUpdateAd( %p ) [no op]\n", ad );
	return true;
}

void
EC2Proc::PublishToEnv( Env * e ) {
	// This doesn't apply to EC2 instances.
	dprintf( D_PERF_TRACE, "EC2Proc::PublishToEnv( %p ) [no op]\n", e );
}

void
EC2Proc::Suspend() {
	// This could apply to EC2 instances, but for now, it doesn't.
	// (i.e., "stop" an instance)
	dprintf( D_PERF_TRACE, "EC2Proc::Suspend() [no op]\n" );
}

void
EC2Proc::Continue() {
	// This could apply to EC2 instances, but for now, it doesn't.
	// (i.e., "Start" an instance)
	dprintf( D_PERF_TRACE, "EC2Proc::Continue() [no op]\n" );
}

bool
EC2Proc::Remove() {
	dprintf( D_PERF_TRACE, "EC2Proc::Remove()\n" );
	return ShutdownFast();
}

bool
EC2Proc::Hold() {
	// We could notionally keep the instance's disk(s) around for a subsequent
	// restart (if we wrote the instance ID back to the schedd), but since we
	// don't do that even for the EC2 universe, let's not bother.
	dprintf( D_PERF_TRACE, "EC2Proc::Hold()\n" );
	return ShutdownFast();
}

bool
EC2Proc::ShutdownGraceful() {
	dprintf( D_PERF_TRACE, "EC2Proc::ShutdownGraceful()\n" );
	return EC2Proc::ShutdownFast();
}

bool
// @return true if shutdown complete, false if pending
EC2Proc::ShutdownFast() {
	dprintf( D_PERF_TRACE, "EC2Proc::ShutdownFast()\n" );

	// FIXME: ...
	return true;
}
