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
	exited_cpu_user_time = 0;
	exited_cpu_sys_time = 0;
	max_image_size = 0;
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
	exited_cpu_user_time = 0;
	exited_cpu_sys_time = 0;
	max_image_size = 0;
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
ProcFamily::get_cpu_usage(long & sys_time, long & user_time)
{
	takesnapshot();
	sys_time = exited_cpu_sys_time + alive_cpu_sys_time;
	user_time = exited_cpu_user_time + alive_cpu_user_time;
}

void 
ProcFamily::get_max_imagesize(unsigned long & max_image )
{
	max_image = max_image_size;
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
	bool currpid_exited;
	bool found_it;
	pid_t pidfamily[2000];

	new_pids = new ExtArray<a_pid>(10);
	newpidindex = 0;

	// On some systems, we can only see process we own, so set_priv
	priv = set_priv(mypriv);

	// grab all pids in the family we can see now
	if ( procAPI->getPidFamily(daddy_pid,pidfamily) < 0 ) {
		// daddy_pid must be gone!
		dprintf( D_PROCFAMILY, 
				 "ProcFamily::takesnapshot: getPidFamily(%d) failed.\n", 
				 daddy_pid );
		pidfamily[0] = 0;
	}
		// Don't need to insert daddypid, it's already in there. 

	// see if there are pids we saw earlier which no longer
	// show up -- they may have been inherited by init
	if ( old_pids ) {

		// loop thru all old pids
		for (i=0;(*old_pids)[i].pid;i++) {

			currpid = (*old_pids)[i].pid;

			// assume pid exited unless we figure out otherwise below
			currpid_exited = true;
			found_it = true;

			// see if currpid exists in our new list
			for (j=0;pidfamily[j] != currpid;j++) {
				if ( pidfamily[j] == 0 ) {
					
					// currpid does not exist in our new pidfamily list !
					found_it = false;

					// So, if currpid still exists on the system, and 
					// the birthdate matches, grab decendants of currpid.

					if ( procAPI->getProcInfo(currpid,pinfo) == 0 ) {
						// compare birthdays; allow 2 seconds "slack"
						birth = (time(NULL) - pinfo->age) - 
									(*old_pids)[i].birthday;
						if ( -2 < birth && birth < 2 ) {
							// birthdays match up; add currpid
							// to our list and also get currpid's decendants
							pidfamily[j] = currpid;
							pidfamily[j+1] = 0;
							procAPI->getPidFamily(currpid,&(pidfamily[j+1]));
							// and this pid most certainly did not exit, so
							// set our flag accordingly.
							currpid_exited = false;
						} 
					} 

					// break out of our for loop, since we have hit the end
					break;
				}
			}	// end of looping through pids in the new list

#ifdef WIN32
			// On Unix, pids get re-used so infrequently that we can make
			// the assumption that a pid will not get re-used in the 
			// relatively short period of time between snapshots.  However,
			// Windows NT reuses pids very very quickly (sigh).  Thus,
			// for Win32 we need to incurr the additional overhead of 
			// comparing birthdates even on pids in the new list which we
			// found in our old list.  This is needed so we can set
			// the currpid_exited correctly to the best we of our knowledge,
			// even if inbetween snapshots the pid exited and got reused as
			// a child pid of some process in our family.  Whew.  
			// Thankfully this will go away in the Win32 port once we
			// start injecting in our DLL into the user job.  -Todd
			if ( found_it ) {
				if ( procAPI->getProcInfo(currpid,pinfo) == 0 ) {
					// compare birthdays; allow 2 seconds "slack"
					birth = (time(NULL) - pinfo->age) - 
								(*old_pids)[i].birthday;
					if ( -2 < birth && birth < 2 ) {
						// birthdays match up, thus since last snapshot
						// this pid most certainly did not exit, so
						// set our flag accordingly.
						currpid_exited = false;
					} 
				} 
			}
#endif	// of ifdef WIN32

			// keep a running tally of the cpu usage for pids
			// which went away.  since the pid is gone from the system,
			// the best we can do is refer back to what we saw in the
			// last snapshot (i.e. the old_pids array).
			if ( currpid_exited) {
				// the current pid exited, so refer to old_pids for time
				exited_cpu_sys_time += (*old_pids)[i].cpu_sys_time;
				exited_cpu_user_time += (*old_pids)[i].cpu_user_time;
			}

		}	// end of looping through all old pids

	}	// if (old_pids)

	// now fill in our new_pids array with all the pids we just found.
	// also tally up the cpu usage for processes still running, and
	// max image size.
	alive_cpu_sys_time = 0;
	alive_cpu_user_time = 0;
	unsigned long curr_image_size = 0;
	for ( j=0; pidfamily[j]; j++ ) {
		if ( procAPI->getProcInfo(pidfamily[j],pinfo) == 0 ) {
			(*new_pids)[newpidindex].pid = pinfo->pid;
			(*new_pids)[newpidindex].ppid = pinfo->ppid;
			(*new_pids)[newpidindex].birthday = time(NULL) - pinfo->age;
			(*new_pids)[newpidindex].cpu_sys_time = pinfo->sys_time;
			(*new_pids)[newpidindex].cpu_user_time = pinfo->user_time;
			alive_cpu_sys_time += pinfo->sys_time;
			alive_cpu_user_time += pinfo->user_time;
#ifdef WIN32
			// On Win32, the imgsize from ProcInfo returns exactly
			// what it says.... this means we get all the bytes mapped
			// into the process image, incl all the DLLs.  This means
			// a little program returns at least 15+ megs.  The ProcInfo
			// rssize is much closer to what the TaskManager reports, 
			// which makes more sense for now.
			curr_image_size += pinfo->rssize;
#else
			curr_image_size += pinfo->imgsize;
#endif
			newpidindex++;
		}
	}
	if ( curr_image_size > max_image_size ) {
		max_image_size = curr_image_size;
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

	if( (DebugFlags & D_PROCFAMILY) && (DebugFlags & D_FULLDEBUG) ) {
		display();
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


void
ProcFamily::display()
{
	int i;
	dprintf( D_PROCFAMILY, "ProcFamily: parent: %d others:", daddy_pid );
	for( i=0; i<family_size; i++ ) {
		dprintf( D_PROCFAMILY | D_NOHEADER, " %d", (*old_pids)[i].pid );
	}
	dprintf( D_PROCFAMILY | D_NOHEADER, "\n" );

	dprintf( D_PROCFAMILY, 
		"ProcFamily: alive_cpu_user = %ld, exited_cpu = %ld, max_image = %luk\n",
		alive_cpu_user_time, exited_cpu_user_time, max_image_size);
}

