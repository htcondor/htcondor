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


// this file includes the parts of ProcAPI that are only needed byt the
// old killfamily code, which is still used if "USE_PROCD = False" in
// condor_config
//

#include "condor_common.h"
#include "condor_debug.h"
#include "procapi.h"
#include "condor_uid.h"

#if defined(WIN32)

#include "ntsysinfo.WINDOWS.h"
static CSysinfo ntSysInfo;

int
ProcAPI::getPidFamily( pid_t daddypid, PidEnvID *penvid, ExtArray<pid_t>& pidFamily, 
	int &status ) 
{

	status = PROCAPI_FAMILY_ALL;

	if ( daddypid == 0 ) {
		pidFamily[0] = 0;
		return PROCAPI_SUCCESS;
	}

	DWORD dwStatus;  // return status of fn. calls

        // '2' is the 'system' , '230' is 'process'  
        // I hope these numbers don't change over time... 
    dwStatus = GetSystemPerfData ( TEXT("230") );
    
	if ( dwStatus != ERROR_SUCCESS ) {
        dprintf( D_ALWAYS,
				 "ProcAPI::getPidFamily failed to get performance "
				 "info (last-error = %u).\n", dwStatus );
		status = PROCAPI_UNSPECIFIED;
        return PROCAPI_FAILURE;
    }

        // somehow we don't have the process data -> panic
    if ( pDataBlock == NULL ) {
        dprintf( D_ALWAYS, 
				 "ProcAPI::getProcSetInfo failed to make pDataBlock.\n");
		status = PROCAPI_UNSPECIFIED;
        return PROCAPI_FAILURE;
    }
    
    PPERF_OBJECT_TYPE pThisObject = firstObject (pDataBlock);

    if ( !offsets )      // If we haven't yet gotten the offsets, grab 'em.
        grabOffsets( pThisObject );
    
	pid_t *allpids;
	pid_t *familypids;
	int numpids, familysize;

	// allpids gets allocated and filled in with pids.  numpids also returned.
	getAllPids( allpids, numpids );

	// returns the familypids resulting from the daddypid
	makeFamily( daddypid, penvid, allpids, numpids, familypids, familysize );
	
	for ( int q=0 ; q<familysize ; q++ ) {
		pidFamily[q] = familypids[q];
	}

	pidFamily[familysize] = 0;

	delete [] allpids;
	delete [] familypids;

	return PROCAPI_SUCCESS;
}

void
ProcAPI::makeFamily( pid_t dadpid, PidEnvID *penvid, pid_t *allpids, 
		int numpids, pid_t* &fampids, int &famsize ) 
{

	pid_t *parentpids = new pid_t[numpids];
	fampids = new pid_t[numpids];  // just might include everyone...

	for( int i=0 ; i<numpids ; i++ ) {
		parentpids[i] = ntSysInfo.GetParentPID( allpids[i] );
	}

	famsize = 1;
	fampids[0] = dadpid;
	int numadditions = 1;

	CSysinfo csi;

	procInfo pi;

	while( numadditions > 0 ) {
		numadditions = 0;
		for( int j=0; j<numpids; j++ ) {
			
			// only need to initialize these aspects of the procInfo for 
			// isinfamily().
			pi.ppid = parentpids[j];
			pi.pid = allpids[j];
			pidenvid_init(&pi.penvid);

			if( isinfamily(fampids, famsize, penvid, &pi) ) {

				// now make sure the parent pid is older than its child
				if (csi.ComparePidAge(allpids[j], parentpids[j]) == -1 ) {

					dprintf(D_FULLDEBUG, "ProcAPI: pid %d is older than "
							"its (presumed) parent (%d), so we're leaving it "
						   "out.\n", allpids[j], parentpids[j]);	

				} else { 
					MSC_SUPPRESS_WARNING_FIXME(6386) // buffer overrun. TJ think's it not a real one though..
					fampids[famsize] = allpids[j];
					parentpids[j] = 0;
					allpids[j] = 0;
					famsize++;
					numadditions++;
				}
			}
		}
	}
	delete [] parentpids;
}

// allocate space for pids and fill in.
void
ProcAPI::getAllPids( pid_t* &pids, int &numpids ) {

    PPERF_OBJECT_TYPE pThisObject = firstObject (pDataBlock);
    PPERF_INSTANCE_DEFINITION pThisInstance = firstInstance(pThisObject);
	numpids = pThisObject->NumInstances;
	pids = new pid_t[numpids];
	UINT_PTR ctrblk;
		
	for( int i=0 ; i<numpids ; i++ ) {
        ctrblk = ((UINT_PTR)pThisInstance) + pThisInstance->ByteLength;
        pids[i] = (pid_t) *((pid_t*)(ctrblk + offsets->procid));
        pThisInstance = nextInstance(pThisInstance);        
	}
}

#else

/* This function returns a list of pids that are 'descendents' of that pid.
     I call this a 'family' of pids.  This list is put into pidFamily, which
     I assume is an already-allocated array.  This array will be terminated
     with a 0 for a pid at its end.  A -1 is returned on failure, 0 otherwise.
*/
int
ProcAPI::getPidFamily( pid_t pid, PidEnvID *penvid, ExtArray<pid_t>& pidFamily, 
	int &status )
{
	int fam_status;
	int rval;

	buildProcInfoList();

	rval = buildFamily(pid, penvid, fam_status);

	switch (rval)
	{
		case PROCAPI_SUCCESS:
			switch (fam_status)
			{
				case PROCAPI_FAMILY_ALL:
					status = PROCAPI_FAMILY_ALL;
					break;

				case PROCAPI_FAMILY_SOME:
					status = PROCAPI_FAMILY_SOME;
					break;

				default:
					/* This shouldn't happen, but if it does */
					EXCEPT( "ProcAPI::buildFamily() returned an "
						"incorrect status on success! Programmer error!\n" );
					break;
			}
			
			break;

		case PROCAPI_FAILURE:

			// no family at all found, clean up and get out 

			deallocAllProcInfos();
			deallocProcFamily();

			status = PROCAPI_FAMILY_NONE;
			return PROCAPI_FAILURE;

			break;

		default:
			break;
	}

	piPTR current = procFamily;  // get descendents and elder pid.
	int i=0;
	while( (current != NULL) ) {
		pidFamily[i] = current->pid;
		i++;
		current = current->next;
	}
  
	pidFamily[i] = 0;

		// deallocate all the lists of stuff...don't leave stale info
		// lying around. 
	deallocAllProcInfos();
	deallocProcFamily();

	return PROCAPI_SUCCESS;
}

piPTR ProcAPI::procFamily	= NULL;

/* buildFamily takes a list of procInfo structs pointed to by 
   allProcInfos and an ancestor pid 'daddypid' and builds a list
   of all procInfo structs and points 'procFamily' to it.
*/
int
ProcAPI::buildFamily( pid_t daddypid, PidEnvID *penvid, int &status ) {

		// array of pids in family.  Size # pids.
	pid_t *familypids;
	int familysize = 0;
	int numprocs;

	// assume that I'm going to find the daddy and all of the descendants...
	status = PROCAPI_FAMILY_ALL;

	if( IsDebugVerbose(D_PROCFAMILY) ) {
		dprintf( D_PROCFAMILY, 
				 "ProcAPI::buildFamily() called w/ parent: %d\n", daddypid );
	}

	numprocs = getNumProcs();
	deallocProcFamily();
	procFamily = NULL;

		// make an array of size # processes for quick lookup of pids in family
	familypids = new pid_t[numprocs];

		// get the daddypid's procInfo struct
	piPTR pred=NULL, current, familyend;

	// find the daddy pid out of the linked list of all of the processes
	current = allProcInfos;
	while( (current != NULL) && (current->pid != daddypid) ) {
		pred = current;
		current = current->next;
	}

	if (current != NULL) {
		dprintf( D_FULLDEBUG, 
			"ProcAPI::buildFamily() Found daddypid on the system: %u\n", 
			current->pid );
	}

	// if the daddy pid isn't in the list, then check and see if there are any
	// children of that pid. If so, pick one to be the conceptual daddy of the
	// rest of them regardless of actual parentage.
	if( current == NULL ) {
		current = allProcInfos;
		while( (current != NULL) && 
				(pidenvid_match(penvid, &current->penvid) != PIDENVID_MATCH))
		{
			pred = current;
			current = current->next;
		}

		if (current != NULL) {

			// found something that was most likely a descendant of daddypid
			status = PROCAPI_FAMILY_SOME;

			dprintf(D_FULLDEBUG, 
				"ProcAPI::buildFamily() Parent pid %u is gone. "
				"Found descendant %u via ancestor environment "
				"tracking and assigning as new \"parent\".\n", 
				daddypid, current->pid);
		}
	}

	// if we couldn't find the daddy pid we were originally looking for, or 
	// any children of said pid (which are members of the daddy pid's family
	// according to the ancestor environment variables condor supplies)
	// then truly give up.
	if( current == NULL ) {

		delete [] familypids;

		dprintf( D_FULLDEBUG, "ProcAPI::buildFamily failed: parent %d "
				 "not found on system.\n", daddypid );

		status = PROCAPI_FAMILY_NONE;

		return PROCAPI_FAILURE;
	}

	// In the case where we truly found the daddy pid, then procFamily will
	// point to it. In the case where we found a child of the daddy pid, then
	// that (arbitrarily chosen) child will become the daddy pid in spirit.

		// special case: daddy is first on list:
	if( allProcInfos == current ) {
		procFamily = allProcInfos;
		allProcInfos = allProcInfos->next;
		procFamily->next = NULL;
	} else {  // regular case: daddy somewhere in middle
		procFamily = current;
		pred->next = current->next;
		procFamily->next = NULL;
	}

	// take whatever we found as a daddy pid proc structure and use that pid.
	// the caller expects the oth index of this array to contain the daddy pid.
	familypids[0] = current->pid;
	familyend = procFamily;
	familysize = 1;

		// now, procFamily points at the procInfo struct for the
		// ancestral pid ( daddypid ).  Its pid is the 0th element in
		// familypids, and familyend will always point to the end of
		// the procFamily list.

	int numadditions = 1;
  
	while( numadditions != 0 ) {
		numadditions = 0;
		current = allProcInfos;
		while( current != NULL ) {
			if( isinfamily(familypids, familysize, penvid, current) ) {
				familypids[familysize] = current->pid;
				familysize++;

				familyend->next = current;

				if( current != allProcInfos ) {
					current = current->next;
					pred->next = current;
				} else {
					current = allProcInfos = allProcInfos->next;          
				}

				familyend = familyend->next;
				familyend->next = NULL;
				
				numadditions++;

			} else {
				pred = current;
				current = current->next;    
			}
		}
	}
	delete [] familypids;

	return PROCAPI_SUCCESS;
}

void
ProcAPI::deallocProcFamily() {
	if( procFamily != NULL ) {
		piPTR prev;
		piPTR temp = procFamily;
		while( temp != NULL ) {
			prev = temp;
			temp = temp->next;
			delete prev;
		}
		procFamily = NULL;
	}
}

/* This function returns the nuber of processes that allProcInfos 
   points at.  It is assumed that allProcInfos points to all the 
   processes in the system ( that buildProcInfoList has been called )
*/
int
ProcAPI::getNumProcs() {
	int nump;
	piPTR temp;
	temp = allProcInfos;
	for( nump=0; temp != NULL; nump++ ) {
		temp = temp->next;
	}
	return nump;
}

#endif

int
ProcAPI::isinfamily( pid_t *fam, int size, PidEnvID *penvid, piPTR child ) 
{
	for( int i=0; i<size; i++ ) {
		// if the child's parent pid is one of the values in the family, then 
		// the child is actually a child of one of the pids in the fam array.
		if( child->ppid == fam[i] ) {

			if( IsDebugVerbose(D_PROCFAMILY) ) {
				dprintf( D_PROCFAMILY, "Pid %u is in family of %u\n", 
					child->pid, fam[i] );
			}

			return true;
		}

		// check to see if the daddy's ancestor pids are a subset of the
		// child's, if so, then the child is a descendent of the daddy pid.
		if (pidenvid_match(penvid, &child->penvid) == PIDENVID_MATCH) {

			if( IsDebugVerbose(D_PROCFAMILY) ) {
				dprintf( D_PROCFAMILY, 
					"Pid %u is predicted to be in family of %u\n", 
					child->pid, fam[i] );
			}

			return true;
		}
	}

	return false;
}

int
ProcAPI::getPidFamilyByLogin( const char *searchLogin, ExtArray<pid_t>& pidFamily )
{
#ifndef WIN32

	// first, get the Login's uid, since that's what's stored in
	// the ProcInfo structure.
	ASSERT(searchLogin);
	struct passwd *pwd = getpwnam(searchLogin);
	if(!pwd) {
		return PROCAPI_FAILURE;
	}
	uid_t searchUid = pwd->pw_uid;

	// now iterate through allProcInfos to find processes
	// owned by the given uid
	piPTR cur = allProcInfos;
	int fam_index = 0;

	buildProcInfoList();

	// buildProcInfoList() just changed allProcInfos pointer, so update cur.
	cur = allProcInfos;

	while ( cur != NULL ) {
		if ( cur->owner == searchUid ) {
			dprintf(D_PROCFAMILY,
				  "ProcAPI: found pid %d owned by %s (uid=%d)\n",
				  cur->pid, searchLogin, searchUid);
			pidFamily[fam_index] = cur->pid;
			fam_index++;
		}
		cur = cur->next;
	}
	pidFamily[fam_index] = (pid_t)0;
	
	return PROCAPI_SUCCESS;
#else
	// Win32 version
	std::vector<pid_t> pids;
	int num_pids;
	int index_pidFamily = 0;
	int index_familyHandles = 0;
	BOOL ret;
	HANDLE procToken;
	HANDLE procHandle;
	TOKEN_INFORMATION_CLASS tic;
	VOID *tokenData;
	DWORD sizeRqd;
	CHAR str[80];
	DWORD strSize;
	CHAR str2[80];
	DWORD str2Size;
	SID_NAME_USE sidType;

	closeFamilyHandles();

	ASSERT(searchLogin);

	// get a list of all pids on the system
	num_pids = ntSysInfo.GetPIDs(pids);

	// loop through pids comparing process owner
	for (size_t s=0; s<pids.size(); s++) {

		// find owner for pid pids[s]
		
	// again, this is assumed to be wrong, so I'm 
	// nixing it for now.
	// skip the daddy_pid, we already added it to our list
//		  if ( pids[s] == daddy_pid ) {
//			  continue;
//		  }

		  // get a handle for the process
		  procHandle=OpenProcess(PROCESS_QUERY_INFORMATION,
			FALSE, pids[s]);
		  if (procHandle == NULL)
		  {
			  // Unable to open the process for query - try next pid
			  continue;			
		  }

		  // get a handle for the access token used
		  // by the process
		  ret=OpenProcessToken(procHandle,
			TOKEN_QUERY, &procToken);
		  if (!ret)
		  {
			  // Unable to open the access token.
			  CloseHandle(procHandle);
			  continue;
		  }

		  // ----- Get user information -----

		  // specify to return user info
		  tic=TokenUser;

		  // find out how much mem is needed
		  ret=GetTokenInformation(procToken, tic, NULL, 0,
			&sizeRqd);

		  // allocate that memory
		  tokenData=(TOKEN_USER *) GlobalAlloc(GPTR,
			sizeRqd);
		  if (tokenData == NULL)
		  {
			EXCEPT("Unable to allocate memory.");
		  }

		  // actually get the user info
		  ret=GetTokenInformation(procToken, tic,
			tokenData, sizeRqd, &sizeRqd);
		  if (!ret)
		  {
			// Unable to get user info.
			CloseHandle(procToken);
			GlobalFree(tokenData);
			CloseHandle(procHandle);
			continue;
		  }

		  // specify size of string buffers
		  strSize=str2Size=80;

		  // convert user SID into a name and domain
		  ret=LookupAccountSid(NULL,
			((TOKEN_USER *)tokenData)->User.Sid,
			str, &strSize, str2, &str2Size, &sidType);
		  if (!ret)
		  {
		    // Unable to look up SID.
			CloseHandle(procToken);
			GlobalFree(tokenData);
			CloseHandle(procHandle);
			continue;
		  }

		  // release memory, handles
		  GlobalFree(tokenData);
		  CloseHandle(procToken);

		  // user is now in variable str, domain in str2

		  // see if it is the login prefix we are looking for
		  if ( strncasecmp(searchLogin,str,strlen(searchLogin))==0 ) {
			  // a match!  add to our list.   
			  pidFamily[index_pidFamily++] = pids[s];
			  // and add the procHandle to a list as well; we keep the
			  // handle open so the pid is not reused by NT between now and
			  // when the caller actually does something with this pid.
			  familyHandles[index_familyHandles++] = procHandle;
			  dprintf(D_PROCFAMILY,
				  "getPidFamilyByLogin: found pid %d owned by %s\n",
				  pids[s],str);
		  } else {
			  // not a match; close the handle to this process
			  CloseHandle(procHandle);
		  }

	}	// end of for loop looping through all pids

	// denote end of list with a zero entry
	pidFamily[index_pidFamily] = 0;

	if ( index_pidFamily > 0 ) {
		// return success
		return PROCAPI_SUCCESS;
	}

	// return failure
	return PROCAPI_FAILURE;

#endif  // of WIN32
}

#if defined(WIN32)

ExtArray<HANDLE> ProcAPI::familyHandles;

void
ProcAPI::closeFamilyHandles()
{
	// This function is a no-op on Unix...
#ifdef WIN32
	int i;
	for ( i=0; i < (familyHandles.getlast() + 1); i++ ) {
		CloseHandle( familyHandles[i] );
	}
	familyHandles.truncate(-1);
#endif // of Win32
	return;
}

// There is a much better way to do this in WIN32...we get all instances
// of all processes at once anyway...
int
ProcAPI::getProcSetInfo( pid_t *pids, int numpids, piPTR& pi, int &status ) {

	// This *could* allocate memory and make pi point to it if 
	// pi == NULL. It is up to the caller to get rid of it.
    initpi( pi );

	status = PROCAPI_OK;

	if( numpids <= 0 || pids == NULL ) {
			// We were passed nothing, so there's no work to do and we
			// should return immediately before we dereference
			// something we shouldn't be touching.
		return PROCAPI_SUCCESS;
	}

	DWORD dwStatus;  // return status of fn. calls

        // '2' is the 'system' , '230' is 'process'  
        // I hope these numbers don't change over time... 
    dwStatus = GetSystemPerfData( TEXT("230") );
    
    if( dwStatus != ERROR_SUCCESS ) {
		dprintf( D_ALWAYS,
			"ProcAPI::getProcSetInfo failed to get performance "
			"info (last-error = %u).\n", dwStatus );
		status = PROCAPI_UNSPECIFIED;
        return PROCAPI_FAILURE;
    }

        // somehow we don't have the process data -> panic
    if( pDataBlock == NULL ) {
        dprintf( D_ALWAYS, "ProcAPI::getProcSetInfo failed to make "
				 "pDataBlock.\n");

		status = PROCAPI_UNSPECIFIED;
        return PROCAPI_FAILURE;
    }
    
    PPERF_OBJECT_TYPE pThisObject = firstObject (pDataBlock);

    if( !offsets ) {	// If we haven't yet gotten the offsets, grab 'em.
        grabOffsets( pThisObject );
    }

        // create local copy of pids.
	pid_t *localpids = (pid_t*) malloc ( sizeof (pid_t) * numpids );
	if (localpids == NULL) {
		EXCEPT( "ProcAPI:getProcSetInfo: Out of Memory!");
	}

	for( int i=0 ; i<numpids ; i++ ) {
		localpids[i] = pids[i];
	}
	
	multiInfo( localpids, numpids, pi );

	free(localpids);

	return PROCAPI_SUCCESS;
}

// We assume that the pDataBlock and offsets are all set up, and we have a
// set of pids (pidslist) to get info on.  Totals returned in pi.
int
ProcAPI::multiInfo( pid_t *pidlist, int numpids, piPTR &pi ) {

    PPERF_OBJECT_TYPE pThisObject = firstObject (pDataBlock);
    PPERF_INSTANCE_DEFINITION pThisInstance = firstInstance(pThisObject);

    double sampleObjectTime = LI_to_double ( pThisObject->PerfTime );
    double objectFrequency  = LI_to_double ( pThisObject->PerfFreq );

		// at this point we're all set to march through the data block to find
        // the pids that we want
    UINT_PTR ctrblk;
	int instanceNum = 0;
    pid_t thispid;
    
	pi->ppid = -1;  // not possible for a set...
	pi->minfault  = 0;  // not supported by NT; all faults lumped into major.
	long maxage = 0;
	long total_faults = 0, faults = 0;
	double total_cpu = 0.0, cpu = 0.0;

	// loop through each instance in data, checking to see if on list

    while( instanceNum < pThisObject->NumInstances ) {
        
        ctrblk = ((UINT_PTR)pThisInstance) + pThisInstance->ByteLength;
        thispid = (pid_t) *((pid_t*)(ctrblk + offsets->procid));

        if( isinlist(thispid, pidlist, numpids) != -1 ) {

            LARGE_INTEGER elt= (LARGE_INTEGER) 
                *((LARGE_INTEGER*)(ctrblk + offsets->elapsed));
			LARGE_INTEGER pt = (LARGE_INTEGER) 
                *((LARGE_INTEGER*)(ctrblk + offsets->pctcpu));
			LARGE_INTEGER ut = (LARGE_INTEGER) 
                *((LARGE_INTEGER*)(ctrblk + offsets->utime));
			LARGE_INTEGER st = (LARGE_INTEGER) 
                *((LARGE_INTEGER*)(ctrblk + offsets->stime));

			pi->pid      =  thispid;
			pi->imgsize  += (long) (*((long*)(ctrblk + offsets->imgsize ))) 
				/ 1024;
			pi->rssize   += (long) (*((long*)(ctrblk + offsets->rssize  ))) 
				/ 1024;
#if HAVE_PSS
#error pssize not handled for this platform
#endif
			pi->user_time+= (long) (LI_to_double( ut ) / objectFrequency);
			pi->sys_time += (long) (LI_to_double( st ) / objectFrequency);
			/* we put the actual ag in here for do_usage_sampling
			   purposes, and then set it to the max at the end */
			pi->age = (long) ((sampleObjectTime - LI_to_double ( elt )) 
                              / objectFrequency);
			if ( pi->age > maxage ) {
				maxage = pi->age;
			}
				/* Creation time of this process, used in identification. */
			pi->creation_time = (long) ((sampleObjectTime/objectFrequency) - 
									((sampleObjectTime - LI_to_double(elt)) /
									 objectFrequency) ); 

			/* We figure out the cpu usage (a total counter, not a 
			   percent!) and the total page faults here. */
            cpu = LI_to_double( pt ) / objectFrequency;
			faults = (long) *((long*)(ctrblk + offsets->faults  ));

			/* for this pid, figure out the %cpu and %faults */
			do_usage_sampling ( pi, cpu, faults, 0, 
				sampleObjectTime/objectFrequency );

			/* stuff these percentages back into a running total */
			total_cpu += pi->cpuusage;
			total_faults += pi->majfault;
			/* ready for use by next pid... */
			pi->cpuusage = 0.0;
			pi->majfault = 0;
		}

    // go to the next one...
		instanceNum++;
        pThisInstance = nextInstance( pThisInstance );        
    }    

	pi->pid = 0;    // It's the sum of many pids...
	pi->age = maxage;  // age is simply the max of the group.  
	pi->creation_time = (long) ((sampleObjectTime/objectFrequency) - maxage );
	pi->cpuusage = total_cpu;   // put our totals in here.
	pi->majfault = total_faults;  // ditto.
    return 0;
}

int ProcAPI::isinlist( pid_t pid, pid_t *pidlist, int numpids ) {
	bool found = false;
	int i = 0;
    
	while ( ( !found ) && ( i < numpids ) ) {
		if ( pidlist[i] == pid )
			found = true;
		else
            i++;
	}
    
	if ( !found ) {
		return -1;
	}
	else {
		return i;
	}
}

#else

/* getProcSetInfo returns the sum of the procInfo structs for a set
   of pids.  These pids are specified by an array of pids ('pids') that
   has 'numpids' elements. */
int
ProcAPI::getProcSetInfo( pid_t *pids, int numpids, piPTR& pi, int &status ) 
{
	piPTR temp = NULL;
	int val = 0;
	priv_state priv;
	int info_status;
	int fatal_failure = FALSE;

	// This *could* allocate memory and make pi point to it if 
	// pi == NULL. It is up to the caller to get rid of it.
	initpi ( pi );

	status = PROCAPI_OK;

	if( numpids <= 0 || pids == NULL ) {
			// We were passed nothing, so there's no work to do and we
			// should return immediately before we dereference
			// something we shouldn't be touching.
		return PROCAPI_SUCCESS;
	}

	priv = set_root_priv();

	for( int i=0; i<numpids; i++ ) {

		val = getProcInfo( pids[i], temp, info_status );

		switch( val ) {
		
			case PROCAPI_SUCCESS:

				pi->imgsize   += temp->imgsize;
				pi->rssize    += temp->rssize;
#if HAVE_PSS
				if( temp->pssize_available ) {
					pi->pssize_available = true;
					pi->pssize    += temp->pssize;
				}
#endif
				pi->minfault  += temp->minfault;
				pi->majfault  += temp->majfault;
				pi->user_time += temp->user_time;
				pi->sys_time  += temp->sys_time;
				pi->cpuusage  += temp->cpuusage;
				if( temp->age > pi->age ) {
					pi->age = temp->age;
				}

				break;

			case PROCAPI_FAILURE:

				switch(info_status) {

					case PROCAPI_NOPID:
						/* We warn about this error, but ignore it since if 
							this pid isn't around, we don't compute any
							usage information for it. */

						dprintf( D_FULLDEBUG, 	
								"ProcAPI::getProcSetInfo(): Pid %d does "
					 			"not exist, ignoring.\n", pids[i] );

						break;

					case PROCAPI_PERM:
						/* The known methods why can happen are that 
							getPidFamily() returned something bogus, or
							a race condition is hit that signifies a
							situation between when a pid was alive in the 
							pidfamily array when this function was entered,
							but exited and was replaced by another pid owned
							by someone else before this for loop got to it
							in the array. So, we'll warn on it, but it won't
							be a fatal error. */

						dprintf( D_FULLDEBUG, 
								"ProcAPI::getProcSetInfo(): Suspicious "
								"permission error getting info for pid %lu.\n",
								(unsigned long)pids[i] );
						break;

					default:
						dprintf( D_ALWAYS, 
							"ProcAPI::getProcSetInfo(): Unspecified return "
					 		"status (%d) from a failed getProcInfo(%lu)\n", 
							info_status, (unsigned long)pids[i] );

						fatal_failure = TRUE;

					break;
				}
			break;

			default:
				EXCEPT("ProcAPI::getProcSetInfo(): Invalid return code. "
						"Programmer error!");
				break;
		}
	}

	if( temp ) { 
		delete temp;
	}

	set_priv( priv );

	if (fatal_failure == TRUE) {
		// Don't really know how to group the various failures of the above code
		status = PROCAPI_UNSPECIFIED;
		return PROCAPI_FAILURE;
	}

	return PROCAPI_SUCCESS;
}

#endif
