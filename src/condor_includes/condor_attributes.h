/* 
** Copyright 1996 by Miron Livny, and Mike Litzkow
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the copyright holders not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the 
** copyright holders make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE COPYRIGHT HOLDERS DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Author: Rajesh Raman 
**
*/ 

// List of attributes used in ClassAd's  If you find yourself using anything
// other than ATTR_<blah> to add/lookup expressions/variables *STOP*, add the
// new variable to this file and use the ATTR_<blah> symbolic constant.  --RR

#ifndef __CONDOR_ATTRIBUTES_H__
#define __CONDOR_ATTRIBUTES_H__

const char ATTR_ARCH                     [] = "Arch";
const char ATTR_CLIENT_MACHINE           [] = "ClientMachine"; 
const char ATTR_CLOCK_DAY                [] = "ClockDay";
const char ATTR_CLOCK_MIN                [] = "ClockMin";
const char ATTR_CONTINUE                 [] = "Continue";
const char ATTR_CPUS                     [] = "Cpus";
const char ATTR_CLUSTER_ID               [] = "ClusterId";
const char ATTR_DISK                     [] = "Disk";
const char ATTR_ENTERED_CURRENT_STATE    [] = "EnteredCurrentState";
const char ATTR_FILE_SYSTEM_DOMAIN       [] = "FileSystemDomain";
const char ATTR_FLAVOR                   [] = "Flavor";
const char ATTR_IDLE_JOBS                [] = "IdleJobs";
const char ATTR_INACTIVE                 [] = "Inactive";
const char ATTR_JOB_PRIO                 [] = "JobPrio";
const char ATTR_JOB_START                [] = "JobStart";
const char ATTR_JOB_STATUS               [] = "JobStatus";
const char ATTR_JOB_UNIVERSE             [] = "JobUniverse";
const char ATTR_KEYBOARD_IDLE            [] = "KeyboardIdle";
const char ATTR_KFLOPS                   [] = "KFlops";
const char ATTR_KILL                     [] = "Kill";
const char ATTR_LAST_HEARD_FROM          [] = "LastHeardFrom";
const char ATTR_LAST_PERIODIC_CHECKPOINT [] = "LastPeriodicCheckpoint";
const char ATTR_LAST_CKPT_SERVER		 [] = "LastCkptServer";
const char ATTR_LOAD_AVG                 [] = "LoadAvg";
const char ATTR_MACHINE                  [] = "Machine";
const char ATTR_MAX_JOBS_RUNNING         [] = "MaxJobsRunning";
const char ATTR_MEMORY                   [] = "Memory";
const char ATTR_MIPS                     [] = "Mips";
const char ATTR_NAME                     [] = "Name";
const char ATTR_NEST                     [] = "Nest";
const char ATTR_NUM_USERS                [] = "NumUsers";
const char ATTR_OPSYS                    [] = "OpSys";
const char ATTR_OWNER                    [] = "Owner"; 
const char ATTR_PERIODIC_CHECKPOINT      [] = "PeriodicCheckpoint";
const char ATTR_PRIO                     [] = "Prio";
const char ATTR_PROC_ID                  [] = "ProcId";
const char ATTR_Q_DATE                   [] = "QDate";
const char ATTR_REMOTE_USER              [] = "RemoteUser";
const char ATTR_REQUIREMENT              [] = "Requirement";
const char ATTR_RUNNING_JOBS             [] = "RunningJobs";
const char ATTR_SCHEDD_IP_ADDR           [] = "ScheddIpAddr";
const char ATTR_SCHEDD_PRIO              [] = "ScheddPrio";
const char ATTR_START                    [] = "Start";
const char ATTR_STARTD_IP_ADDR           [] = "StartdIpAddr";
const char ATTR_STATE                    [] = "State";
const char ATTR_STATUS					 [] = "Status";
const char ATTR_SUBNET                   [] = "Subnet";
const char ATTR_SUSPEND                  [] = "Suspend";
const char ATTR_UID_DOMAIN               [] = "UidDomain";
const char ATTR_UPDATE_PRIO              [] = "UpdatePrio";
const char ATTR_VACATE                   [] = "Vacate";
const char ATTR_VIRTUAL_MEMORY           [] = "VirtualMemory";
const char ATTR_LAST_UPDATE				 [] = "LastUpdate";

#endif
