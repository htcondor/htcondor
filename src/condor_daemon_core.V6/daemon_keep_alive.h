/***************************************************************
 *
 * Copyright (C) 1990-2008, Condor Team, Computer Sciences Department,
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


#ifndef _CONDOR_DAEMON_KEEP_ALIVE_H_
#define _CONDOR_DAEMON_KEEP_ALIVE_H_

/** This class contains all the logic to maintain a keep-alive heartbeat
	between a daemoncore parent and its children, enabling a parent to
	kill children that appear to be hung.  
	These methods used to be embedded into the DaemonCore class itself,
	but now they have been pulled out into this DaemonKeepAlive helper
	class for clarity.
	This class is only intended to be used by DaemonCore, and in fact
	is an integral part of DaemonCore in that it is a friend class and
	accesses private structures in DaemonCore.
*/
class DaemonKeepAlive: public Service {
	friend class DaemonCore;

protected:
	DaemonKeepAlive();
	~DaemonKeepAlive();

	/**
	   Set whether this daemon should should send CHILDALIVE commands
	   to its daemoncore parent. Must be called from
	   main_pre_command_sock_init() to have any effect. The default
	   is to send CHILDALIVEs.
	*/
	void WantSendChildAlive( bool send_alive )
		{ m_want_send_child_alive = send_alive; }

	void reconfig(void);
	bool get_stats();
	int HandleChildAliveCommand(int command, Stream* stream);

private:
	int SendAliveToParent() const;
	void SendAliveToParentFromTimer() { (void)SendAliveToParent(); }
	int KillHungChild(void* pidentry);
	int ScanForHungChildren();
	void ScanForHungChildrenFromTimer() { (void)ScanForHungChildren(); }

	int max_hang_time;
	int max_hang_time_raw;
	int m_child_alive_period;
	int send_child_alive_timer;
	int scan_for_hung_children_timer;
	bool m_want_send_child_alive;

};

#endif
