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

#include "condor_common.h"
#include "killfamily.h"
#include "../condor_procapi/procapi.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
static char *_FileName_ = __FILE__;         // Used by EXCEPT (see except.h)

// We do all this stuff here so killfamily.C can be linked in
// with program using DaemonCore as well as programs which are
// completely DaemonCore ignorant (like the old starter).
#ifdef WANT_DC_PM
#	ifdef SIGCONT
#		undef SIGCONT
#	endif
#	define SIGCONT DC_SIGCONT

#	ifdef SIGSTOP
#		undef SIGSTOP
#	endif
#	define SIGSTOP DC_SIGSTOP

#	ifdef SIGKILL
#		undef SIGKILL
#	endif
#	define SIGKILL DC_SIGKILL
#endif // of WANT_DC_PM

ProcFamily::ProcFamily( pid_t pid, priv_state priv, int test_only )
{
	daddy_pid = pid;
	procAPI = new ProcAPI;
	needs_free = 1;
	old_pids = NULL;
	mypriv = priv;
	test_only_flag = test_only;
	family_size = 0;
}

ProcFamily::ProcFamily( pid_t pid, priv_state priv, ProcAPI* papi )
{
	daddy_pid = pid;
	procAPI = papi;
	needs_free = 0;
	old_pids = NULL;
	mypriv = priv;
	test_only_flag = 0;
	family_size = 0;
}

ProcFamily::~ProcFamily()
{
	if( needs_free ) {
		delete procAPI;
	}
	if ( old_pids ) {
		delete old_pids;
	}
}

void
ProcFamily::hardkill()
{
	takesnapshot();
	spree(SIGKILL, INFANTICIDE);
}

void
ProcFamily::suspend()
{
	takesnapshot();
	spree(SIGSTOP, PATRICIDE);
}

void
ProcFamily::resume()
{
		// Shouldn't need to take a snapshot, since if the family is
		// suspended, it hasn't been changing.
	spree(SIGCONT, INFANTICIDE);
}

void
ProcFamily::softkill( int sig )
{
	takesnapshot();
	spree(SIGCONT, INFANTICIDE);
	spree(sig, INFANTICIDE);
}

void
ProcFamily::safe_kill(pid_t inpid,int sig)
{
	priv_state priv;

	// make certain we do not kill init or worse!
	if ( inpid < 2 || daddy_pid < 2 ) {
		if ( test_only_flag ) {
			printf(
				"ProcFamily::safe_kill: attempt to kill pid %d!\n",inpid);
			} else {
			dprintf(D_ALWAYS,
				"ProcFamily::safe_kill: attempt to kill pid %d!\n",inpid);
			}
		return;
	}

	priv = set_priv(mypriv);

	if ( test_only_flag ) {
		printf(
			"ProcFamily::safe_kill: about to kill pid %d with sig %d\n",
			inpid, sig);
	} else {
		dprintf(D_FULLDEBUG,
			"ProcFamily::safe_kill: about to kill pid %d with sig %d\n",
			inpid, sig);
	}

	if ( !test_only_flag ) {
#ifdef WANT_DC_PM
		if ( daemonCore->Send_Signal(inpid,sig) == FALSE ) {
			dprintf(D_FULLDEBUG,
				"ProcFamily::safe_kill: Send_Signal(%d,%d) failed\n",
				inpid,sig);
		}
#else
		if ( kill(inpid,sig) < 0 ) {
			dprintf(D_FULLDEBUG,
				"ProcFamily::safe_kill: kill(%d,%d) failed, errno=%d\n",
				inpid,sig,errno);
		}
#endif
	}

	set_priv(priv);
}

void
ProcFamily::spree(int sig,KILLFAMILY_DIRECTION direction)
{
	int start = 0;
	int i = -1;
	int j;

	do {
		i++;
		if ( (*old_pids)[i].ppid == 1 || (*old_pids)[i].pid == 0 ) {
			if ( direction == PATRICIDE ) {
				// PATRICIDE: parents go first
				for (j=start;j<i;j++) {
					safe_kill((*old_pids)[j].pid,sig);
				}
			} else {
				// INFANTICIDE: kids go first
				for (j=i-1;j>=start;j--) {
					safe_kill((*old_pids)[j].pid,sig);
				}
			}
			start = i;
		}
	} while ( (*old_pids)[i].pid );
}

void
ProcFamily::takesnapshot()
{
	ExtArray<a_pid> *new_pids;
	struct procInfo *pinfo = NULL;
	int i,j,newpidindex;
	pid_t currpid;
	long birth;
	priv_state priv;
	pid_t pidfamily[2000];

	new_pids = new ExtArray<a_pid>(10);
	newpidindex = 0;

	// On some systems, we can only see process we own, so set_priv
	priv = set_priv(mypriv);

	// grab all pids in the family we can see now
	if ( procAPI->getPidFamily(daddy_pid,pidfamily) < 0 ) {
		// daddy_pid must be gone!
		pidfamily[0] = 0;
	}
		// Don't need to insert daddypid, it's already in there. 

	// see if there are pids we saw earlier which no longer
	// show up -- they may have been inherited by init
	if ( old_pids ) {

		// loop thru all old pids
		for (i=0;(*old_pids)[i].pid;i++) {

			currpid = (*old_pids)[i].pid;

			// skip our daddy_pid, cuz getPidFamily() only returns
			// decendants, not the daddy_pid itself.
			if (currpid == daddy_pid)
				continue;

			// see if currpid exists in our new list
			for (j=0;pidfamily[j] != currpid;j++) {
				if ( pidfamily[j] == 0 ) {
					
					// currpid does not exist in our new pidfamily list !
					// So, if currpid still exists on the system, and 
					// the birthdate matches, grab decendants of currpid.

					if ( procAPI->getProcInfo(currpid,pinfo) == 0 ) {
						// compare birthdays; allow 3 seconds "slack"
						birth = (time(NULL) - pinfo->age) - 
									(*old_pids)[i].birthday;
						if ( -3 < birth && birth < 3 ) {
							// birthdays match up; add currpid
							// to our list and also get currpid's decendants
							pidfamily[j] = currpid;
							pidfamily[j+1] = 0;
							procAPI->getPidFamily(currpid,&(pidfamily[j+1]));
						}
					}

					// break out of our for loop, since we have hit the end
					break;
				}
			}

		}
	}

	// now fill in our new_pids array with all the pids we just found
	for ( j=0; pidfamily[j]; j++ ) {
		if ( procAPI->getProcInfo(pidfamily[j],pinfo) == 0 ) {
			(*new_pids)[newpidindex].pid = pinfo->pid;
			(*new_pids)[newpidindex].ppid = pinfo->ppid;
			(*new_pids)[newpidindex].birthday = time(NULL) - pinfo->age;
			newpidindex++;
		}
	}

	// we are done!  replace the old_pids array stored in our class with
	// the new updated new_pids array we just computed.
	if ( old_pids ) {
		delete old_pids;
	}
	old_pids = new_pids;

		// Record the new size of our pid family, (which is
		// conveniently stored in newpidindex already).
	family_size = newpidindex;

	// getProcInfo() allocates memory; free it
	if ( pinfo ) {
		delete pinfo;
	}

	// set our priv state back to what it originally was
	set_priv(priv);
}


int
ProcFamily::currentfamily( pid_t* & ptr  )
{
	pid_t* tmp;
	int i;
	tmp = new pid_t[ family_size ];
	if( !tmp ) {
		EXCEPT( "Out of memory!" );
	}
	for( i=0; i<family_size; i++ ) {
		tmp[i] = (*old_pids)[i].pid;
	}
	ptr = tmp;
	return family_size;
}
