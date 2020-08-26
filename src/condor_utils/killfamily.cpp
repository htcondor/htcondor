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


#include "condor_common.h"
#include "killfamily.h"
#include "../condor_procapi/procapi.h"

KillFamily::KillFamily( pid_t pid, priv_state priv, int test_only )
{
	daddy_pid = pid;
	old_pids = NULL;
	mypriv = priv;
	test_only_flag = test_only;
	family_size = 0;
	exited_cpu_user_time = 0;
	exited_cpu_sys_time = 0;
	max_image_size = 0;

	alive_cpu_sys_time = 0;
	alive_cpu_user_time = 0;

	pidenvid_init(&m_penvid);
	searchLogin = NULL;

#ifdef WIN32
	// On Win32, all KillFamily activity needs to be done as LocalSystem
	mypriv = PRIV_ROOT;
#endif

	dprintf( D_PROCFAMILY,
			 "Created new KillFamily w/ pid %d as parent\n", daddy_pid );
}


KillFamily::~KillFamily()
{
	if ( old_pids ) {
		delete old_pids;
	}
	if ( searchLogin ) {
		free(searchLogin);
	}
	dprintf( D_PROCFAMILY,
			 "Deleted KillFamily w/ pid %d as parent\n", daddy_pid );
}

void
KillFamily::setFamilyEnvironmentID( PidEnvID* penvid )
{
	if (penvid != NULL) {
		pidenvid_copy(&m_penvid, penvid);
	}
}

void
KillFamily::setFamilyLogin( const char *login )
{
	if ( login ) {
		if ( searchLogin )
			free(searchLogin);
		searchLogin = strdup(login);
	}
}

void
KillFamily::hardkill()
{
	dprintf(D_PROCFAMILY,"Entering KillFamily::hardkill\n");
	takesnapshot();
	spree(SIGKILL, INFANTICIDE);
}

void
KillFamily::suspend()
{
	dprintf(D_PROCFAMILY,"Entering KillFamily::suspend\n");
	takesnapshot();
	spree(SIGSTOP, PATRICIDE);
}

void
KillFamily::resume()
{
		// Shouldn't need to take a snapshot, since if the family is
		// suspended, it hasn't been changing.
	dprintf(D_PROCFAMILY,"Entering KillFamily::resume\n");
	spree(SIGCONT, INFANTICIDE);
}

void
KillFamily::softkill( int sig )
{
	dprintf(D_PROCFAMILY,"Entering KillFamily::softkill sig=%d\n",sig);
	takesnapshot();
	spree(SIGCONT, INFANTICIDE);
	spree(sig, INFANTICIDE);
}

void
KillFamily::get_cpu_usage(long & sys_time, long & user_time)
{
	takesnapshot();
	sys_time = exited_cpu_sys_time + alive_cpu_sys_time;
	user_time = exited_cpu_user_time + alive_cpu_user_time;
}

void
KillFamily::get_max_imagesize(unsigned long & max_image ) const
{
	max_image = max_image_size;
}

void
KillFamily::safe_kill(a_pid *pid, int sig)
{
	priv_state priv;
	pid_t inpid;

	inpid = pid->pid;

	// make certain we do not kill init or worse!
	if ( inpid < 2 || daddy_pid < 2 ) {
		if ( test_only_flag ) {
			printf(
				"KillFamily::safe_kill: attempt to kill pid %d!\n",inpid);
			} else {
			dprintf(D_ALWAYS,
				"KillFamily::safe_kill: attempt to kill pid %d!\n",inpid);
			dprintf(D_PROCFAMILY,
				"KillFamily::safe_kill: attempt to kill pid %d!\n",inpid);

			}
		return;
	}

	priv = set_priv(mypriv);

	if ( test_only_flag ) {
		printf(
			"KillFamily::safe_kill: about to kill pid %d with sig %d\n",
			inpid, sig);
	} else {
		dprintf(D_PROCFAMILY,
			"KillFamily::safe_kill: about to kill pid %d with sig %d\n",
			inpid, sig);
	}

	if ( !test_only_flag ) {

#ifdef WIN32

		HANDLE pHnd;
		piPTR pi;
		int status;

		pi = NULL;

		pHnd = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, inpid);

		if ( pHnd == NULL ) {
			dprintf(D_ALWAYS, "Procfamily: ERROR: Could not open pid %d "
				"(err=%d). Maybe it exited already?\n", inpid, GetLastError());
		}

		if ( ProcAPI::getProcInfo(inpid, pi, status) == PROCAPI_SUCCESS ) {

			if ( daemonCore->Send_Signal(inpid,sig) == FALSE ) {
				dprintf(D_PROCFAMILY,
					"KillFamily::safe_kill: Send_Signal(%d,%d) failed\n",
					inpid,sig);
			}

		} else {

			// Procapi didn't know anything about this pid, so
			// we assume it has exited on its own.

			dprintf(D_PROCFAMILY, "Procfamily: getProcInfo() failed to "
					"get info for pid %d, so it is presumed dead.\n", inpid);
		}

		if ( pi ) {
			delete pi;
		}

		if ( pHnd) {
		   	CloseHandle(pHnd);
		}

#else
		if ( kill(inpid,sig) < 0 ) {
			dprintf(D_PROCFAMILY,
				"KillFamily::safe_kill: kill(%d,%d) failed, errno=%d\n",
				inpid,sig,errno);
		}
#endif // WIN32
	}

	set_priv(priv);
}


void
KillFamily::spree(int sig,KILLFAMILY_DIRECTION direction)
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
KillFamily::takesnapshot()
{
	ExtArray<a_pid> *new_pids;
	struct procInfo *pinfo = NULL;
	int i,j,newpidindex;
	pid_t currpid;
	priv_state priv;
	bool currpid_exited;
	bool found_it;
	int ret_val;
	int fam_status;
	int info_status;
	int ignore_status;

	ExtArray<pid_t> pidfamily;

	new_pids = new ExtArray<a_pid>;
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
		ret_val = ProcAPI::getPidFamilyByLogin(searchLogin,pidfamily);
	} else {
		ret_val = ProcAPI::getPidFamily(daddy_pid,&m_penvid,pidfamily,fam_status);
	}

	if ( ret_val == PROCAPI_FAILURE ) {
		// daddy_pid AND any detectable family must be gone from the set!
		dprintf( D_PROCFAMILY,
				 "KillFamily::takesnapshot: getPidFamily(%d) failed. "
				 "Could not find the pid or any family members.\n",
				 daddy_pid );
		pidfamily[0] = 0;
	}

	// Don't need to insert daddypid, it's already in there,
	// unless the daddy pid is gone, but procapi found a family
	// for it anyway via the ancestor variables...

	// see if there are pids we saw earlier which no longer
	// show up -- they may have been inherited by init.
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

					// If the above call to get the family KNEW that
					// the daddy pid was already exited (since it couldn't be
					// found) but yet it returned something it thought was
					// family anyway, check against the old pids but don't
					// consider the daddy pid to even be alive.

					if ( ProcAPI::getProcInfo(currpid,pinfo,info_status)
							== PROCAPI_SUCCESS )
					{
						// compare birthdays
						if ( pinfo->birthday == (*old_pids)[i].birthday ) {

							// birthdays match up; add currpid
							// to our list and also get currpid's decendants
							pidfamily[j] = currpid;
							j++;

							// if we got the pid family via login name,
							// we alrady have the decendants.
							if ( !searchLogin ) {
								ExtArray<pid_t> detached_family;
								detached_family[0] = 0;
								if (ProcAPI::getPidFamily(currpid,&m_penvid,detached_family,ignore_status) != PROCAPI_FAILURE) {
									for (int k = 0; detached_family[k] != 0; k++) {
										if (detached_family[k] != currpid) {
											pidfamily[j] = detached_family[k];
											j++;
										}
									}
										// If we found the family, this pid
										// is still here.
									currpid_exited = false;
								} else {
										// If we failed to get the pid family,
										// the pid is most likely gone.
									currpid_exited = true;
								}
							} else {
								// and this pid most certainly did not exit,
							    // so set our flag accordingly.
								currpid_exited = false;
							}
							pidfamily[j] = 0;
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
				if ( ProcAPI::getProcInfo(currpid,pinfo,ignore_status)
						== PROCAPI_SUCCESS )
			{
					if ( pinfo->birthday != (*old_pids)[i].birthday ) {
						// birthdays match up, thus since last snapshot
						// this pid most certainly did not exit, so
						// set our flag accordingly.
						currpid_exited = true;
					}
				}
			}
#else
			if (found_it) {} // To get rid of set-but-not-used warning
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
		if ( ProcAPI::getProcInfo(pidfamily[j],pinfo,ignore_status)
				== PROCAPI_SUCCESS )
		{
			(*new_pids)[newpidindex].pid = pinfo->pid;
			(*new_pids)[newpidindex].ppid = pinfo->ppid;
			(*new_pids)[newpidindex].birthday = pinfo->birthday;
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

	if( IsDebugVerbose(D_PROCFAMILY) ) {
		display();
	}

	// set our priv state back to what it originally was
	set_priv(priv);
}


int
KillFamily::currentfamily( pid_t* & ptr  )
{
	pid_t* tmp;
	int i;

	if( family_size <= 0 ) {
		/* This used to be an ERROR, but we can't figure out why since the
			above layers of code can end up calling this in the situation
			when a pid is exited, reaped by waitpid, but not yet by the
			daemoncore reaper. So, I've moved it to WARNING instead so
			we can still think about it when it happens in the codebase
			instead of forgetting about it. See ticket #706.
		*/
		dprintf( D_ALWAYS,
				 "KillFamily::currentfamily: WARNING: family_size is non-positive (%d)\n", family_size);
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
KillFamily::display()
{
	int i;
	dprintf( D_PROCFAMILY, "KillFamily: parent: %d family:", daddy_pid );
	for( i=0; i<family_size; i++ ) {
		dprintf( D_PROCFAMILY | D_NOHEADER, " %d", (*old_pids)[i].pid );
	}
	dprintf( D_PROCFAMILY | D_NOHEADER, "\n" );

	dprintf( D_PROCFAMILY,
		"KillFamily: alive_cpu_user = %ld, exited_cpu = %ld, max_image = %luk\n",
		alive_cpu_user_time, exited_cpu_user_time, max_image_size);
}
