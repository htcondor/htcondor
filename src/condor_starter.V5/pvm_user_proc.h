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
