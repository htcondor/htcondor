/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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


#ifndef _CONDOR_COMMANDS_H
#define _CONDOR_COMMANDS_H

/****
** Queue Manager Commands
****/
#define QMGMT_CMD	1111

/* Scheduler Commands */
/*
**	Scheduler version number
*/
#define SCHED_VERS			400
#define CA_AUTH_CMD_BASE	1000
// beware, QMGMT_CMD is 1111, so we don't want to use 1100...
#define CA_CMD_BASE			1200
#define ALT_STARTER_BASE 	70

/*
**	In the following definitions 'FRGN' does not
**	stand for "friggin'"...
*/
#define CONTINUE_FRGN_JOB	(SCHED_VERS+1)
#define CONTINUE_CLAIM		(SCHED_VERS+1)		// New name for CONTINUE_FRGN_JOB
#define SUSPEND_FRGN_JOB	(SCHED_VERS+2)
#define SUSPEND_CLAIM		(SCHED_VERS+2)		// New name for SUSPEND_FRGN_JOB
#define CKPT_FRGN_JOB		(SCHED_VERS+3)		
#define DEACTIVATE_CLAIM	(SCHED_VERS+3)		// New name for CKPT_FRGN_JOB
#define KILL_FRGN_JOB		(SCHED_VERS+4)
#define DEACTIVATE_CLAIM_FORCIBLY	(SCHED_VERS+4)		// New name for KILL_FRGN_JOB

#define LOCAL_STATUS		(SCHED_VERS+5)
#define LOCAL_STATISTICS	(SCHED_VERS+6)

#define PERMISSION			(SCHED_VERS+7)
#define SET_DEBUG_FLAGS		(SCHED_VERS+8)
#define PREEMPT_LOCAL_JOBS	(SCHED_VERS+9)

#define RM_LOCAL_JOB		(SCHED_VERS+10)
#define START_FRGN_JOB		(SCHED_VERS+11)

#define AVAILABILITY		(SCHED_VERS+12)		/* Not used */
#define NUM_FRGN_JOBS		(SCHED_VERS+13)
#define STARTD_INFO			(SCHED_VERS+14)
#define SCHEDD_INFO			(SCHED_VERS+15)
#define NEGOTIATE			(SCHED_VERS+16)
#define SEND_JOB_INFO		(SCHED_VERS+17)
#define NO_MORE_JOBS		(SCHED_VERS+18)
#define JOB_INFO			(SCHED_VERS+19)
#define GIVE_STATUS			(SCHED_VERS+20)
#define RESCHEDULE			(SCHED_VERS+21)
#define PING				(SCHED_VERS+22)
#define NEGOTIATOR_INFO		(SCHED_VERS+23)
#define GIVE_STATUS_LINES	(SCHED_VERS+24)
#define END_NEGOTIATE		(SCHED_VERS+25)
#define REJECTED			(SCHED_VERS+26)
#define X_EVENT_NOTIFICATION		(SCHED_VERS+27)
#define RECONFIG			(SCHED_VERS+28)
#define GET_HISTORY			(SCHED_VERS+29)
#define UNLINK_HISTORY_FILE			(SCHED_VERS+30)
#define UNLINK_HISTORY_FILE_DONE	(SCHED_VERS+31)
#define DO_NOT_UNLINK_HISTORY_FILE	(SCHED_VERS+32)
#define SEND_ALL_JOBS		(SCHED_VERS+33)
#define SEND_ALL_JOBS_PRIO	(SCHED_VERS+34)
#define REQ_NEW_PROC		(SCHED_VERS+35)
#define PCKPT_FRGN_JOB		(SCHED_VERS+36)
#define SEND_RUNNING_JOBS	(SCHED_VERS+37)
#define CHECK_CAPABILITY    (SCHED_VERS+38)
#define GIVE_PRIORITY		(SCHED_VERS+39)
#define	MATCH_INFO			(SCHED_VERS+40)
#define	ALIVE				(SCHED_VERS+41)
#define REQUEST_CLAIM 		(SCHED_VERS+42)
#define REQUEST_SERVICE		(SCHED_VERS+42)		// Old name for REQUEST_CLAIM
#define RELEASE_CLAIM 		(SCHED_VERS+43)
#define RELINQUISH_SERVICE	(SCHED_VERS+43)		// Old name for RELEASE_CLAIM
#define VACATE_SERVICE		(SCHED_VERS+43)		// Old name for RELEASE_CLAIM
#define ACTIVATE_CLAIM	 	(SCHED_VERS+44)
#define PRIORITY_INFO       (SCHED_VERS+45)     /* negotiator to accountant */
#define PCKPT_ALL_JOBS		(SCHED_VERS+46)
#define VACATE_ALL_CLAIMS	(SCHED_VERS+47)
#define GIVE_STATE			(SCHED_VERS+48)
#define SET_PRIORITY		(SCHED_VERS+49)		// negotiator(priviliged) cmd 
#define GIVE_CLASSAD		(SCHED_VERS+50)
#define GET_PRIORITY		(SCHED_VERS+51)		// negotiator
#define GIVE_REQUEST_AD		(SCHED_VERS+52)		// Starter -> Startd
#define RESTART				(SCHED_VERS+53)
#define DAEMONS_OFF			(SCHED_VERS+54)
#define DAEMONS_ON			(SCHED_VERS+55)
#define MASTER_OFF			(SCHED_VERS+56)
#define CONFIG_VAL			(SCHED_VERS+57)
#define RESET_USAGE			(SCHED_VERS+58)		// negotiator
#define SET_PRIORITYFACTOR	(SCHED_VERS+59)		// negotiator
#define RESET_ALL_USAGE		(SCHED_VERS+60)		// negotiator
#define DAEMONS_OFF_FAST	(SCHED_VERS+61)
#define MASTER_OFF_FAST		(SCHED_VERS+62)
#define GET_RESLIST			(SCHED_VERS+63)		// negotiator
#define ATTEMPT_ACCESS		(SCHED_VERS+64) 	// schedd, test a file
#define VACATE_CLAIM		(SCHED_VERS+65)     // vacate a given claim
#define PCKPT_JOB			(SCHED_VERS+66)     // periodic ckpt a given VM
#define DAEMON_OFF			(SCHED_VERS+67)		// specific daemon, subsys follows 
#define DAEMON_OFF_FAST		(SCHED_VERS+68)		// specific daemon, subsys follows 
#define DAEMON_ON			(SCHED_VERS+69)		// specific daemon, subsys follows 
#define GIVE_TOTALS_CLASSAD	(SCHED_VERS+70)
#define DUMP_STATE          (SCHED_VERS+71)	// drop internal vars into classad
#define PERMISSION_AND_AD	(SCHED_VERS+72) // negotiator is sending startad to schedd
#define REQUEST_NETWORK		(SCHED_VERS+73)	// negotiator network mgmt
#define VACATE_ALL_FAST		(SCHED_VERS+74)		// fast vacate for whole machine
#define VACATE_CLAIM_FAST	(SCHED_VERS+75)  	// fast vacate for a given VM
#define REJECTED_WITH_REASON (SCHED_VERS+76) // diagnostic version of REJECTED
#define START_AGENT			(SCHED_VERS+77) // have the master start an agent
#define ACT_ON_JOBS			(SCHED_VERS+78) // have the schedd act on some jobs (rm, hold, release)
#define STORE_CRED			(SCHED_VERS+79)		// schedd, store a credential
#define SPOOL_JOB_FILES		(SCHED_VERS+80)	// spool all job files via filetransfer object
#define GET_MYPROXY_PASSWORD (SCHED_VERS+81) // gmanager->schedd: Give me MyProxy password
#define DELETE_USER			(SCHED_VERS+82)		// negotiator  (actually, accountant)
#define DAEMON_OFF_PEACEFUL  (SCHED_VERS+83)		// specific daemon, subsys follows
#define DAEMONS_OFF_PEACEFUL (SCHED_VERS+84)
#define RESTART_PEACEFUL     (SCHED_VERS+85)
#define TRANSFER_DATA		(SCHED_VERS+86) // send all job files back via filetransfer object
#define UPDATE_GSI_CRED		(SCHED_VERS+87) // send refreshed gsi proxy file
#define SPOOL_JOB_FILES_WITH_PERMS	(SCHED_VERS+88)	// spool all job files via filetransfer object (new version with file permissions)
#define TRANSFER_DATA_WITH_PERMS	(SCHED_VERS+89) // send all job files back via filetransfer object (new version with file permissions)
#define CHILD_ON            (SCHED_VERS+90) // Turn my child ON (HAD)
#define CHILD_OFF           (SCHED_VERS+91) // Turn my child OFF (HAD)
#define CHILD_OFF_FAST      (SCHED_VERS+92) // Turn my child OFF/Fast (HAD)
#define NEGOTIATE_WITH_SIGATTRS	(SCHED_VERS+93)	// same as NEGOTIATE, but send sig attrs after Owner
#define SET_ACCUMUSAGE	(SCHED_VERS+94)		// negotiator
#define SET_BEGINTIME	(SCHED_VERS+95)		// negotiator
#define SET_LASTTIME	(SCHED_VERS+96)		// negotiator
#define STORE_POOL_CRED		(SCHED_VERS+97)	// master, store password for daemon-to-daemon shared secret auth (PASSWORD)

/*
  The ClassAd-only protocol.  CA_CMD is the base command that's sent
  on the wire that means "read a ClassAd off the wire, lookup
  ATTR_COMMAND, do the right thing, and send the results back as a
  ClassAd".  The rest of the commands listed here are possible values
  for ATTR_COMMAND.
  CA_AUTH_CMD forces authentication if that's needed, while CA_CMD
  just uses whatever authentication methods are configured.
*/

#define CA_AUTH_CMD                  (CA_AUTH_CMD_BASE+0) 

// generic claiming protocol that the startd uses for COD
#define CA_REQUEST_CLAIM        (CA_AUTH_CMD_BASE+1)
#define CA_RELEASE_CLAIM        (CA_AUTH_CMD_BASE+2)
#define CA_ACTIVATE_CLAIM       (CA_AUTH_CMD_BASE+3)
#define CA_DEACTIVATE_CLAIM     (CA_AUTH_CMD_BASE+4)
#define CA_SUSPEND_CLAIM        (CA_AUTH_CMD_BASE+5)
#define CA_RESUME_CLAIM         (CA_AUTH_CMD_BASE+6)
// other commands that use the ClassAd-only protocol 
// CA_LOCATE_STARTER used to be (CA_AUTH_CMD_BASE+7), but no more 
// CA_RECONNECT_JOB used to be  (CA_AUTH_CMD_BASE+8), but no more 

#define CA_CMD                  (CA_CMD_BASE+0) 
#define CA_LOCATE_STARTER       (CA_CMD_BASE+1)
#define CA_RECONNECT_JOB        (CA_CMD_BASE+2)

/************
*** Command ids used by the collector 
************/
const int UPDATE_STARTD_AD		= 0;
const int UPDATE_SCHEDD_AD		= 1;
const int UPDATE_MASTER_AD		= 2;
const int UPDATE_GATEWAY_AD		= 3;
const int UPDATE_CKPT_SRVR_AD	= 4;

const int QUERY_STARTD_ADS		= 5;
const int QUERY_SCHEDD_ADS		= 6;
const int QUERY_MASTER_ADS		= 7;
const int QUERY_GATEWAY_ADS		= 8;
const int QUERY_CKPT_SRVR_ADS	= 9;
const int QUERY_STARTD_PVT_ADS	= 10;

const int UPDATE_SUBMITTOR_AD	= 11;
const int QUERY_SUBMITTOR_ADS	= 12;

const int INVALIDATE_STARTD_ADS	= 13;
const int INVALIDATE_SCHEDD_ADS	= 14;
const int INVALIDATE_MASTER_ADS	= 15;
const int INVALIDATE_GATEWAY_ADS	= 16;
const int INVALIDATE_CKPT_SRVR_ADS	= 17;
const int INVALIDATE_SUBMITTOR_ADS	= 18;

const int UPDATE_COLLECTOR_AD	= 19;
const int QUERY_COLLECTOR_ADS	= 20;
const int INVALIDATE_COLLECTOR_ADS	= 21;

const int QUERY_HIST_STARTD = 22;
const int QUERY_HIST_STARTD_LIST = 23;
const int QUERY_HIST_SUBMITTOR = 24;
const int QUERY_HIST_SUBMITTOR_LIST = 25;
const int QUERY_HIST_GROUPS = 26;
const int QUERY_HIST_GROUPS_LIST = 27;
const int QUERY_HIST_SUBMITTORGROUPS = 28;
const int QUERY_HIST_SUBMITTORGROUPS_LIST = 29;
const int QUERY_HIST_CKPTSRVR = 30;
const int QUERY_HIST_CKPTSRVR_LIST = 31;

const int UPDATE_LICENSE_AD			= 42;
const int QUERY_LICENSE_ADS			= 43;
const int INVALIDATE_LICENSE_ADS	= 44;

const int UPDATE_STORAGE_AD = 45;
const int QUERY_STORAGE_ADS = 46;
const int INVALIDATE_STORAGE_ADS = 47;

const int QUERY_ANY_ADS = 48;

const int UPDATE_NEGOTIATOR_AD 	= 49;
const int QUERY_NEGOTIATOR_ADS	= 50;
const int INVALIDATE_NEGOTIATOR_ADS = 51;

const int UPDATE_QUILL_AD	= 52;
const int QUERY_QUILL_ADS	= 53;
const int INVALIDATE_QUILL_ADS  = 54;

const int UPDATE_HAD_AD = 55;
const int QUERY_HAD_ADS = 56;
const int INVALIDATE_HAD_ADS = 57;

const int UPDATE_AD_GENERIC = 58;
const int INVALIDATE_ADS_GENERIC = 59;

/*
*** Daemon Core Signals
*/


// Signals used for Startd -> Starter communication
#define DC_SIGSUSPEND	100
#define DC_SIGCONTINUE	101
#define DC_SIGSOFTKILL	102	// vacate w/ checkpoint
#define DC_SIGHARDKILL	103 // kill w/o checkpoint
#define DC_SIGPCKPT		104	// periodic checkpoint
#define DC_SIGREMOVE	105
#define DC_SIGHOLD		106

/*
*** Daemon Core Commands and Signals
*/
#define DC_BASE	60000
#define DC_RAISESIGNAL		(DC_BASE+0)
#define DC_PROCESSEXIT		(DC_BASE+1)
#define DC_CONFIG_PERSIST	(DC_BASE+2)
#define DC_CONFIG_RUNTIME	(DC_BASE+3)
#define DC_RECONFIG			(DC_BASE+4)
#define DC_OFF_GRACEFUL		(DC_BASE+5)
#define DC_OFF_FAST			(DC_BASE+6)
#define DC_CONFIG_VAL		(DC_BASE+7)
#define DC_CHILDALIVE		(DC_BASE+8)
#define DC_SERVICEWAITPIDS	(DC_BASE+9) 
#define DC_AUTHENTICATE     (DC_BASE+10)
#define DC_NOP              (DC_BASE+11)
#define DC_RECONFIG_FULL	(DC_BASE+12)
#define DC_FETCH_LOG        (DC_BASE+13)
#define DC_INVALIDATE_KEY   (DC_BASE+14)
#define DC_OFF_PEACEFUL     (DC_BASE+15)
#define DC_SET_PEACEFUL_SHUTDOWN (DC_BASE+16)
#define DC_TIME_OFFSET      (DC_BASE+17)

/*
*** Log type supported by DC_FETCH_LOG
*** These are not interpreted directly by DaemonCore,
*** so it's ok that they start at zero.
*/

#define DC_FETCH_LOG_TYPE_PLAIN 0
  /* Add more type here... */

/*
*** Result codes given by DC_FETCH_LOG.
*** These are not interpreted directly by DaemonCore,
*** so it's ok that they start at zero.
*/

#define DC_FETCH_LOG_RESULT_SUCCESS   0
#define DC_FETCH_LOG_RESULT_NO_NAME   1
#define DC_FETCH_LOG_RESULT_CANT_OPEN 2
#define DC_FETCH_LOG_RESULT_BAD_TYPE  3

/*
*** Commands used by the FileTransfer object
*/
#define FILETRANSFER_BASE 61000
#define FILETRANS_UPLOAD (FILETRANSFER_BASE+0)
#define FILETRANS_DOWNLOAD (FILETRANSFER_BASE+1)

/*
*** Condor Password Daemon Commands
*/
#define PW_BASE 70000
#define PW_SETPASS			(PW_BASE+1)
#define PW_GETPASS			(PW_BASE+2)
#define PW_CLEARPASS		(PW_BASE+3)

/*
*** Commands used by the daemon core Shadow
*/
#define DCSHADOW_BASE 71000
#define SHADOW_UPDATEINFO	   (DCSHADOW_BASE+0)
#define TAKE_MATCH             (DCSHADOW_BASE+1)  // for MPI & parallel shadow
#define MPI_START_COMRADE      (DCSHADOW_BASE+2)  // for MPI & parallel shadow
#define GIVE_MATCHES 	       (DCSHADOW_BASE+3)  // for MPI & parallel shadow
#define RECEIVE_JOBAD		   (DCSHADOW_BASE+4)


#define STORK_BASE 80000
#define STORK_SUBMIT (STORK_BASE+0)
#define STORK_REMOVE (STORK_BASE+1)
#define STORK_STATUS (STORK_BASE+2)
#define STORK_LIST 	 (STORK_BASE+3)

#define CREDD_BASE 81000	
#define CREDD_STORE_CRED (CREDD_BASE+0)
#define CREDD_GET_CRED (CREDD_BASE+1)
#define CREDD_REMOVE_CRED (CREDD_BASE+2)
#define CREDD_QUERY_CRED (CREDD_BASE+3)
#define CREDD_GET_PASSWD (CREDD_BASE+99)	// used by the Win32 credd only

/*
*** Used only in THE TOOL to choose the condor_squawk option.
*/
#define SQUAWK 72000

/*
*** Commands used by the gridmanager daemon
*/
#define DCGRIDMANAGER_BASE 73000
#define GRIDMAN_CHECK_LEASES (DCGRIDMANAGER_BASE+0)
#define GRIDMAN_REMOVE_JOBS SIGUSR1
#define GRIDMAN_ADD_JOBS SIGUSR2

/*
*** Replies used in various stages of various protocols
*/

/* Failure cases */
#ifndef NOT_OK 
#define NOT_OK		0
#endif
#ifndef REJECTED
#define REJECTED	0
#endif

/* Success cases */
#ifndef OK
#define OK			1
#endif
#ifndef ACCEPTED
#define ACCEPTED	1
#endif

/* Other replies */
#define CONDOR_TRY_AGAIN	2
#define CONDOR_ERROR	3


#endif  /* of ifndef _CONDOR_COMMANDS_H */
