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
#include "killfamily.h"
#include "../condor_procapi/procapi.h"
#include "dynuser.h"

#ifdef WIN32
extern dynuser *myDynuser;
#endif

ProcFamily::ProcFamily( pid_t pid, priv_state priv, int test_only )
{
	daddy_pid = pid;
	old_pids = NULL;
	mypriv = priv;
	test_only_flag = test_only;
	family_size = 0;
	exited_cpu_user_time = 0;
	exited_cpu_sys_time = 0;
	max_image_size = 0;

	searchLogin = NULL;

#ifdef WIN32
	// On Win32, all ProcFamily activity needs to be done as LocalSystem
	mypriv = PRIV_ROOT;
#endif

	dprintf( D_PROCFAMILY, 
			 "Created new ProcFamily w/ pid %d as parent\n", daddy_pid );
}


ProcFamily::~ProcFamily()
{
	if ( old_pids ) {
		delete old_pids;
	}
	if ( searchLogin ) {
		free(searchLogin);
	}
	dprintf( D_PROCFAMILY,
			 "Deleted ProcFamily w/ pid %d as parent\n", daddy_pid ); 
}

void
ProcFamily::setFamilyLogin( const char *login )
{
	if ( login ) {
		if ( searchLogin ) 
			free(searchLogin);
		searchLogin = strdup(login);
	}
}

void
ProcFamily::hardkill()
{
	dprintf(D_PROCFAMILY,"Entering ProcFamily::hardkill\n");
	takesnapshot();
	spree(SIGKILL, INFANTICIDE);
}

void
ProcFamily::suspend()
{
	dprintf(D_PROCFAMILY,"Entering ProcFamily::suspend\n");
	takesnapshot();
	spree(SIGSTOP, PATRICIDE);
}

void
ProcFamily::resume()
{
		// Shouldn't need to take a snapshot, since if the family is
		// suspended, it hasn't been changing.
	dprintf(D_PROCFAMILY,"Entering ProcFamily::resume\n");
	spree(SIGCONT, INFANTICIDE);
}

void
ProcFamily::softkill( int sig )
{
	dprintf(D_PROCFAMILY,"Entering ProcFamily::softkill sig=%d\n",sig);
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
ProcFamily::safe_kill(a_pid *pid, int sig)
{
	priv_state priv;
	pid_t inpid;

	inpid = pid->pid;

	// make certain we do not kill init or worse!
	if ( inpid < 2 || daddy_pid < 2 ) {
		if ( test_only_flag ) {
			printf(
				"ProcFamily::safe_kill: attempt to kill pid %d!\n",inpid);
			} else {
			dprintf(D_ALWAYS,
				"ProcFamily::safe_kill: attempt to kill pid %d!\n",inpid);
			dprintf(D_PROCFAMILY,
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
		dprintf(D_PROCFAMILY,
			"ProcFamily::safe_kill: about to kill pid %d with sig %d\n",
			inpid, sig);
	}

	if ( !test_only_flag ) {

#ifdef WIN32

		// on Win32, we have to make sure that when we kill something,
		// the process we're about to kill is the one in our last snapshot,
		// not a new process that just happened to pick up the same pid.
		// So, first we open up the pid's handle. As long as we
		// have a handle to the pid in question, its pid will not
		// get reused in the event it exits while we're doing all
		// of this. Next, we'll check that its birthday is the
		// same as it was when we took the last snapshot. If all goes 
		// well, we may proceed in killing the process, and finally 
		// close the handle.
		// -stolley 5/2004

		HANDLE pHnd;
		piPTR pi;

		pi = NULL;

		pHnd = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, inpid);

		if ( pHnd == NULL ) {
			dprintf(D_ALWAYS, "Procfamily: ERROR: Could not open pid %d "
				"(err=%d)\n", inpid, GetLastError());
		}

		ProcAPI::getProcInfo(inpid, pi);

		int birth = (time(NULL) - pi->age);

			// check if process birthday is atleast as old as the last
			// snapshot's value
		if ( !(birth <= pid->birthday) ) {
			dprintf(D_ALWAYS, "Procfamily: Not killing pid %d "
				"birthday (%li) not less than or equal to value from "
				"last snapshot (%li)\n",
				inpid, birth, pid->birthday);

		} else if ( daemonCore->Send_Signal(inpid,sig) == FALSE ) {
			dprintf(D_PROCFAMILY,
				"ProcFamily::safe_kill: Send_Signal(%d,%d) failed\n",
				inpid,sig);
		}

		if ( pi ) {
			delete pi;
		}
		
		CloseHandle(pHnd);

#else
		if ( kill(inpid,sig) < 0 ) {
			dprintf(D_PROCFAMILY,
				"ProcFamily::safe_kill: kill(%d,%d) failed, errno=%d\n",
				inpid,sig,errno);
		}
#endif // WIN32
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
					safe_kill(&(*old_pids)[j], sig);
				}
			} else {
				// INFANTICIDE: kids go first
				for (j=i-1;j>=start;j--) {
					safe_kill(&(*old_pids)[j], sig);
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
	int ret_val;
	pid_t pidfamily[2000];

	new_pids = new ExtArray<a_pid>(10);
	newpidindex = 0;

	// On some systems, we can only see process we own, so we must be either
	// the user or root. However, being the user in this function causes many,
	// many priv state changes from user to root and back again since 
	// getProcInfo changes to root(and back again) to look at the pid. This
	// smashes the NIS master with too many calls and the load on it
	// skyrockets so, we are root here. The real way to solve this problem is
	// to cache the groups in the code so changes from/to the user uid don't
	// talk to the NIS master. But that is for another day. -psilord 8/22/02
	priv = set_root_priv();

	// grab all pids in the family we can see now
	if ( searchLogin ) {
		ret_val = ProcAPI::getPidFamilyByLogin(searchLogin, pidfamily);
	} else {
		ret_val = ProcAPI::getPidFamily(daddy_pid,pidfamily);
	}

	if ( ret_val < 0 ) {
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
			found_it = false;

			// see if currpid exists in our new list
			for( j=0 ;; j++ ) {
				if( pidfamily[j] == currpid ) {
						// Found it, so it certainly didn't exit!
					currpid_exited = false;
					found_it = true;
						// Now that we've found this pid in the new
						// list, we can break out of the for() loop,
					break;
				}
				if( pidfamily[j] == 0 ) {
					
					// currpid does not exist in our new pidfamily list !
					found_it = false;

					// So, if currpid still exists on the system, and 
					// the birthdate matches, grab decendants of currpid.

					if ( ProcAPI::getProcInfo(currpid,pinfo) == 0 ) {
						// compare birthdays; allow 2 seconds "slack"
						birth = (time(NULL) - pinfo->age) - 
									(*old_pids)[i].birthday;
						if ( -2 < birth && birth < 2 ) {
							// birthdays match up; add currpid
							// to our list and also get currpid's decendants
							pidfamily[j] = currpid;
							pidfamily[j+1] = 0;
							// if we got the pid family via login name,
							// we alrady have the decendants.
							if ( !searchLogin ) {  
								ProcAPI::getPidFamily(currpid,&(pidfamily[j+1]));
							}
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
				if ( ProcAPI::getProcInfo(currpid,pinfo) == 0 ) {
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
		if ( ProcAPI::getProcInfo(pidfamily[j],pinfo) == 0 ) {
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

	if( family_size <= 0 ) {
		dprintf( D_ALWAYS, 
				 "ProcFamily::currentfamily: ERROR: family_size is 0\n" );
		ptr = NULL;
		return 0;
	}
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
	dprintf( D_PROCFAMILY, "ProcFamily: parent: %d family:", daddy_pid );
	for( i=0; i<family_size; i++ ) {
		dprintf( D_PROCFAMILY | D_NOHEADER, " %d", (*old_pids)[i].pid );
	}
	dprintf( D_PROCFAMILY | D_NOHEADER, "\n" );

	dprintf( D_PROCFAMILY, 
		"ProcFamily: alive_cpu_user = %ld, exited_cpu = %ld, max_image = %luk\n",
		alive_cpu_user_time, exited_cpu_user_time, max_image_size);
}
