/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
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
