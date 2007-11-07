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


 

#include "user_proc.h"

#define	TIDPVMD		0x80000000		/* Stolen from PVM's global.h */

extern int		shadow_tid;

class PVMdProc:public UserProc {
public:
	PVMdProc( V3_PROC &p, char *exec, char *orig, char *targ, uid_t u, uid_t g, int id , int soft) : 
		UserProc(p, exec, orig, targ, u, g, id, soft) { shadow_tid = p.notification;
											  dprintf( D_ALWAYS, "setting shadow_tid to t%x\n", p.notification); }
	void execute();
	void delete_files();
	void suspend() {}
	void resume() {}
};

class PVMUserProc:public UserProc {
public:
	PVMUserProc( V3_PROC &p, char *exec, char *orig, char *targ, uid_t u, uid_t g, int id , int soft) : 
		UserProc(p, exec, orig, targ, u, g, id, soft) {}
	void execute();
	void send_sig( int );

	int		get_tid() { return pvm_tid; }
private:
	int		pvm_tid;
};
