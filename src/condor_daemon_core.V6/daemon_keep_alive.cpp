/***************************************************************
 *
 * Copyright (C) 1990-2017, Condor Team, Computer Sciences Department,
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
#include "condor_daemon_core.h"
#include "subsystem_info.h"
#include "condor_config.h"
#include "condor_email.h"

DaemonKeepAlive::DaemonKeepAlive()
{
	max_hang_time = -1;
	max_hang_time_raw = 3600;
	m_child_alive_period = -1;
    send_child_alive_timer = -1;
	scan_for_hung_children_timer = -1;
	m_want_send_child_alive = true;
}

DaemonKeepAlive::~DaemonKeepAlive()
{

}

void
DaemonKeepAlive::reconfig(void) 
{
	// NOTE: this function is always called on initial startup, as well
	// as at reconfig time.

	// Setup a timer to send child keepalives to our parent, if we have
	// a daemon core parent.
	if ( daemonCore->ppid && m_want_send_child_alive ) {
		std::string buf;
		int old_max_hang_time_raw = max_hang_time_raw;
		formatstr(buf,"%s_NOT_RESPONDING_TIMEOUT",get_mySubSystem()->getName());
		max_hang_time_raw = param_integer(buf.c_str(),param_integer("NOT_RESPONDING_TIMEOUT",3600,1),1);

		if( max_hang_time_raw != old_max_hang_time_raw || send_child_alive_timer == -1 ) {
			max_hang_time = max_hang_time_raw + timer_fuzz(max_hang_time_raw);
				// timer_fuzz() should never make it <= 0
			ASSERT( max_hang_time > 0 );
		}
		int old_child_alive_period = m_child_alive_period;
		m_child_alive_period = (max_hang_time / 3) - 30;
		if ( m_child_alive_period < 1 )
			m_child_alive_period = 1;
		if ( send_child_alive_timer == -1 ) {

				// 2008-06-18 7.0.3: commented out direct call to
				// SendAliveToParent(), because it causes deadlock
				// between the shadow and schedd if the job classad
				// that the schedd is writing over a pipe to the
				// shadow is larger than the pipe buffer size.
				// For now, register timer for 0 seconds instead
				// of calling SendAliveToParent() immediately.
				// This means we are vulnerable to a race condition,
				// in which we hang before the first CHILDALIVE.  If
				// that happens, our parent will never kill us.

			send_child_alive_timer = daemonCore->Register_Timer(0,
					(unsigned)m_child_alive_period,
					(TimerHandlercpp)&DaemonKeepAlive::SendAliveToParentFromTimer,
					"DaemonKeepAlive::SendAliveToParent", this );

				// Send this immediately, because if we hang before
				// sending this message, our parent will not kill us.
				// (Commented out.  See reason above.)
				// SendAliveToParent();
		} else if( m_child_alive_period != old_child_alive_period ) {
				// Our parent will not know about our new alive period
				// until the next time we send it an ALIVE message, so
				// we can't just increase the time to our next call
				// without risking being killed as a hung child.
			daemonCore->Reset_Timer(send_child_alive_timer, 1, m_child_alive_period);
		}
	}

	// Setup a timer to scan for hung child processes.
	// We will default to scanning every 60 seconds, but if this
	// takes more than 1% of our time we degrade to every 10 minutes.
	if ( scan_for_hung_children_timer == -1 ) {
		Timeslice interval;
		interval.setDefaultInterval(60);
		interval.setMinInterval(1);
		interval.setMaxInterval(600);
		interval.setTimeslice(0.01);
		scan_for_hung_children_timer = daemonCore->Register_Timer(interval,
				(TimerHandlercpp)&DaemonKeepAlive::ScanForHungChildrenFromTimer,
				"DaemonKeepAlive::ScanForHungChildren", this );
	}

}

bool
DaemonKeepAlive::get_stats()
{
	return true;
}

int
DaemonKeepAlive::ScanForHungChildren()
{
	unsigned int now = (unsigned int)time(NULL);

	DaemonCore::PidEntry *pid_entry;
	daemonCore->pidTable->startIterations();
	while( daemonCore->pidTable->iterate(pid_entry) ) {
		if( pid_entry &&
			pid_entry->hung_past_this_time &&
			now > pid_entry->hung_past_this_time )
		{
			KillHungChild(pid_entry);
		}
	}

	return TRUE;
}


int 
DaemonKeepAlive::SendAliveToParent() const
{
	std::string parent_sinful_string_buf;
	char const *parent_sinful_string;
	char const *tmp;
	int ret_val;
	static bool first_time = true;
	int number_of_tries = 3;

	dprintf(D_FULLDEBUG,"DaemonKeepAlive: in SendAliveToParent()\n");

	pid_t ppid = daemonCore->ppid;

	if ( !ppid ) {
		// no daemon core parent, nothing to send
		return FALSE;
	}

		/* Don't have the CGAHP and/or DAGMAN, which are launched as the user,
		   attempt to send keep alives to daemon. Permissions are not likely to
		   allow user proccesses to send signals to Condor daemons. 
		   Note that we shouldn't have to check for DAGMan here as no
		   daemon core info should have been inherited down to DAGMan, 
		   but it doesn't hurt to be sure here since we already need
		   to check for the CGAHP. */
	if (get_mySubSystem()->isType(SUBSYSTEM_TYPE_GAHP) ||
	  	get_mySubSystem()->isType(SUBSYSTEM_TYPE_DAGMAN))
	{
		return FALSE;
	}

		/* Before we possibly block trying to send this alive message to our 
		   parent, lets see if this parent pid (ppid) exists on this system.
		   This protects, for instance, against us acting a bogus CONDOR_INHERIT
		   environment variable that perhaps just got inherited down through
		   the ages.
		*/
	if ( !daemonCore->Is_Pid_Alive(ppid) ) {
		dprintf(D_FULLDEBUG,
			"DaemonKeepAlive: in SendAliveToParent() - ppid %ul disappeared!\n",
			ppid);
		return FALSE;
	}

	tmp = daemonCore->InfoCommandSinfulString(ppid);
	if ( tmp ) {
			// copy the result from InfoCommandSinfulString,
			// because the pointer we got back is a static buffer
		parent_sinful_string_buf = tmp;
		parent_sinful_string = parent_sinful_string_buf.c_str();
	} else {
		dprintf(D_FULLDEBUG,"DaemonKeepAlive: No parent_sinful_string. "
			"SendAliveToParent() failed.\n");
			// parent already gone?
		return FALSE;
	}

	double dprintf_lock_delay = dprintf_get_lock_delay();
	dprintf_reset_lock_delay();

	bool blocking = first_time;
	classy_counted_ptr<Daemon> d = new Daemon(DT_ANY,parent_sinful_string);
	classy_counted_ptr<ChildAliveMsg> msg = new ChildAliveMsg(daemonCore->mypid,max_hang_time,number_of_tries,dprintf_lock_delay,blocking);

	int timeout = m_child_alive_period / number_of_tries;
	if( timeout < 60 ) {
		timeout = 60;
	}
	msg->setDeadlineTimeout( timeout );
	msg->setTimeout( timeout );

	if( blocking || !d->hasUDPCommandPort() || !daemonCore->m_wants_dc_udp ) {
		msg->setStreamType( Stream::reli_sock );
	}
	else {
		msg->setStreamType( Stream::safe_sock );
	}

	if( blocking ) {
		d->sendBlockingMsg( msg.get() );
		ret_val = msg->deliveryStatus() == DCMsg::DELIVERY_SUCCEEDED;
	}
	else {
		d->sendMsg( msg.get() );
		ret_val = TRUE;
	}

	if ( first_time ) {
		first_time = false;
		if ( ret_val == FALSE ) {
			EXCEPT("FAILED TO SEND INITIAL KEEP ALIVE TO OUR PARENT %s",
				parent_sinful_string);
		}
	}

	if (ret_val == FALSE) {
		dprintf(D_ALWAYS,"DaemonKeepAlive: Leaving SendAliveToParent() - "
			"FAILED sending to %s\n",
			parent_sinful_string);
	} else if( msg->deliveryStatus() == DCMsg::DELIVERY_SUCCEEDED ) {
		dprintf(D_FULLDEBUG,"DaemonKeepAlive: Leaving SendAliveToParent() - success\n");
	} else {
		dprintf(D_FULLDEBUG,"DaemonKeepAlive: Leaving SendAliveToParent() - pending\n");
	}

	return TRUE;
}

int 
DaemonKeepAlive::HandleChildAliveCommand(int, Stream* stream)
{
	pid_t child_pid = 0;
	unsigned int timeout_secs = 0;
	DaemonCore::PidEntry *pidentry;
	double dprintf_lock_delay = 0.0;

	if (!stream->code(child_pid) ||
		!stream->code(timeout_secs)) {
		dprintf(D_ALWAYS,"Failed to read ChildAlive packet (1)\n");
		return FALSE;
	}

		// There is an optional additional dprintf_lock_delay in the
		// message.  It is optional so that external programs can send
		// simple alive messages using condor_squawk.
	if( stream->peek_end_of_message() ) {
		if( !stream->end_of_message() ) {
			dprintf(D_ALWAYS,"Failed to read ChildAlive packet (2)\n");
			return FALSE;
		}
	}
	else if( !stream->code(dprintf_lock_delay) ||
			 !stream->end_of_message())
	{
		dprintf(D_ALWAYS,"Failed to read ChildAlive packet (3)\n");
		return FALSE;
	}


	if ((daemonCore->pidTable->lookup(child_pid, pidentry) < 0)) {
		// we have no information on this pid
		dprintf(D_ALWAYS,
			"Received child alive command from unknown pid %d\n",child_pid);
		return FALSE;
	}

	pidentry->hung_past_this_time = (unsigned int) time(NULL) + timeout_secs;
	pidentry->was_not_responding = FALSE;
	pidentry->got_alive_msg += 1;

	dprintf(D_DAEMONCORE,
			"received childalive, pid=%d, secs=%d, dprintf_lock_delay=%f\n",child_pid,timeout_secs,dprintf_lock_delay);

		/* The Lock Convoy of Shadowy Doom.  Once upon a time, there
		 * was a submit machine that melted down for hours with high
		 * load and no way for the admins to bring it back to normal
		 * without rebooting it.  There were 30k-40k shadows running.
		 * During that time, some of the shadows waited for hours to
		 * get a lock to the shadow log, while others experienced much
		 * less wait time, indicating a high degree of unfairness.
		 * Further analysis revealed that the system was likely
		 * spending most of its time context switching whenever the
		 * lock was freed and was rarely actually getting any real
		 * work done.  This is known as the lock convoy problem.
		 *
		 * To address this, we made unix daemons append without
		 * locking (using O_APPEND).  A lock is still used for
		 * rotation, but this should not lead to the lock convoy
		 * problem unless rotation is happening in a matter of
		 * seconds.  Just in case the shadowy convoy ever returns,
		 * we want to leave some better clues behind.
		 */

	if( dprintf_lock_delay > 0.01 ) {
		dprintf(D_ALWAYS,"WARNING: child process %d reports that it has spent %.1f%% of its time waiting for a lock to its log file.  This could indicate a scalability limit that could cause system stability problems.\n",child_pid,dprintf_lock_delay*100);
	}
	if( dprintf_lock_delay > 0.1 ) {
			// things are looking serious, so let's send mail
		static time_t last_email = 0;
		if( last_email == 0 || time(NULL)-last_email > 60 ) {
			last_email = time(NULL);

			std::string subject;
			formatstr(subject,"Condor process reports long locking delays!");

			FILE *mailer = email_admin_open(subject.c_str());
			if( mailer ) {
				fprintf(mailer,
						"\n\nThe %s's child process with pid %d has spent %.1f%% of its time waiting\n"
						"for a lock to its log file.  This could indicate a scalability limit\n"
						"that could cause system stability problems.\n",
						get_mySubSystem()->getName(),
						child_pid,
						dprintf_lock_delay*100);
				email_close( mailer );
			}
		}
	}

	return TRUE;
}

int 
DaemonKeepAlive::KillHungChild(void *child)
{
	if (!child) return FALSE;
	DaemonCore::PidEntry *pidentry = (DaemonCore::PidEntry *) child;
	pid_t hung_child_pid = pidentry->pid;
	ASSERT( hung_child_pid > 1 );

	if( daemonCore->ProcessExitedButNotReaped( hung_child_pid ) ) {
			// This process has exited, but we have not gotten around to
			// reaping it yet.  Do nothing.
		dprintf(D_FULLDEBUG,"Canceling hung child timer for pid %d, because it has exited but has not been reaped yet.\n",hung_child_pid);
		return FALSE;
	}

	// set a flag in the PidEntry so a reaper can discover it was killed
	// because it was hung.
	bool first_time = true;
	if( pidentry->was_not_responding ) {
		first_time = false;
	} else {
		pidentry->was_not_responding = TRUE;
	}

	dprintf(D_ALWAYS,"ERROR: Child pid %d appears hung! Killing it hard.\n",
		hung_child_pid);

	// and hardkill the bastard!
	bool want_core = param_boolean( "NOT_RESPONDING_WANT_CORE", false );

#ifndef WIN32
	if( want_core ) {
		// On multiple occassions, I have observed the child process
		// get hung while writing its core file.  If we never follow
		// up with a hard-kill, this can result in the service going
		// down for days, which is terrible.  Therefore, set things up
		// so we will get invoked again later to follow up with a hard-kill if
		// the child gets hung dumping core.
		if( !first_time ) {
			dprintf(D_ALWAYS,
					"Child pid %d is still hung!  Perhaps it hung while generating a core file.  Killing it harder.\n",hung_child_pid);
			want_core = false;
		}
		else {
			dprintf(D_ALWAYS, "Sending SIGABRT to child to generate a core file.\n");
			const unsigned int want_core_timeout = 600;
			pidentry->hung_past_this_time = (unsigned int) time(NULL) + want_core_timeout;
		}
	}
#endif

	return daemonCore->Shutdown_Fast(hung_child_pid, want_core );
}

