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

#include "os_proc.h"
#include "ec2_proc.h"

EC2Proc::EC2Proc( ClassAd * jobAd ) : OsProc( jobAd ) {
	dprintf( D_PERF_TRACE, "EC2Proc::EC2Proc( %p )\n", JobAd );
}

EC2Proc::~EC2Proc() {
	dprintf( D_PERF_TRACE, "EC2Proc::~EC2Proc()\n" );
}

int
EC2Proc::StartJob() {
	dprintf( D_PERF_TRACE, "EC2Proc::StartJob()\n" );

	// FIXME: Start a captive grid manager with JobAd on standard in.
	// Note that the ad's JobUniverse must be changed to 9 for the
	// grid manager to be happy.

	// FIXME: Set JobPid to the captive grid manager we just started...
	return false;
}

bool
EC2Proc::JobReaper( int pid, int status ) {
	dprintf( D_PERF_TRACE, "EC2Proc::JobReaper( %d, %d )\n", pid, status );

	// FIXME: If the grid manager exited...
	return false;
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
