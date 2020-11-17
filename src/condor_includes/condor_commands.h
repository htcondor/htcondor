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



#ifndef _CONDOR_COMMANDS_H
#define _CONDOR_COMMANDS_H

/* these control the command_table_generator.pl script that makes the command id <-> name lookup tables.
NAMETABLE_DIRECTIVE:CLASS:BTranslation
NAMETABLE_DIRECTIVE:TABLE:DCTranslation
*/

/****
** Queue Manager Commands
****/
#define QMGMT_BASE		1110
#define QMGMT_READ_CMD	(QMGMT_BASE+1)
#define QMGMT_WRITE_CMD	(QMGMT_BASE+2)

/* Scheduler Commands */
/*
**	Scheduler version number
*/
#define SCHED_VERS			400
#define HAD_COMMANDS_BASE                  (700)
#define REPLICATION_COMMANDS_BASE          (800)
#define CA_AUTH_CMD_BASE	1000
// beware, QMGMT_CMD is 1111, so we don't want to use 1100...
#define CA_CMD_BASE			1200

/*
**	In the following definitions 'FRGN' does not
**	stand for "friggin'"...
*/
#define CONTINUE_CLAIM		(SCHED_VERS+1)		// New name for CONTINUE_FRGN_JOB
#define SUSPEND_CLAIM		(SCHED_VERS+2)		// New name for SUSPEND_FRGN_JOB
#define DEACTIVATE_CLAIM	(SCHED_VERS+3)		// New name for CKPT_FRGN_JOB
#define DEACTIVATE_CLAIM_FORCIBLY	(SCHED_VERS+4)		// New name for KILL_FRGN_JOB

#define LOCAL_STATUS		(SCHED_VERS+5)		/* Not used */
//#define LOCAL_STATISTICS	(SCHED_VERS+6)		/* Not used */

#define PERMISSION			(SCHED_VERS+7)		// used in negotiation protocol
//#define SET_DEBUG_FLAGS		(SCHED_VERS+8)		/* Not used */
//#define PREEMPT_LOCAL_JOBS	(SCHED_VERS+9)		/* Not used */

//#define RM_LOCAL_JOB		(SCHED_VERS+10)		/* Not used */
//#define START_FRGN_JOB		(SCHED_VERS+11)		/* Not used */

//#define AVAILABILITY		(SCHED_VERS+12)		/* Not used */
//#define NUM_FRGN_JOBS		(SCHED_VERS+13)		/* Not used */
//#define STARTD_INFO			(SCHED_VERS+14)		/* Not used */
//#define SCHEDD_INFO			(SCHED_VERS+15)		/* Not used */
#define NEGOTIATE			(SCHED_VERS+16) // 7.5.4+ negotiation command
#define SEND_JOB_INFO		(SCHED_VERS+17)     // used in negotiation protocol
#define NO_MORE_JOBS		(SCHED_VERS+18)		// used in negotiation protocol
#define JOB_INFO			(SCHED_VERS+19)		// used in negotiation protocol
//#define GIVE_STATUS			(SCHED_VERS+20)		/* Not used */
#define RESCHEDULE			(SCHED_VERS+21)
//#define PING				(SCHED_VERS+22)			/* Not used */
//#define NEGOTIATOR_INFO		(SCHED_VERS+23)		/* Not used */
//#define GIVE_STATUS_LINES	(SCHED_VERS+24)			/* Not used */
#define END_NEGOTIATE		(SCHED_VERS+25)		// used in negotiation protocol
#define REJECTED			(SCHED_VERS+26)
#define X_EVENT_NOTIFICATION		(SCHED_VERS+27)
//#define RECONFIG			(SCHED_VERS+28)			/* Not used */
#define GET_HISTORY			(SCHED_VERS+29)		/* repurposed in 8.9.7 to be query startd history */
//#define UNLINK_HISTORY_FILE			(SCHED_VERS+30)	/* Not used */
//#define UNLINK_HISTORY_FILE_DONE	(SCHED_VERS+31)		/* Not used */
//#define DO_NOT_UNLINK_HISTORY_FILE	(SCHED_VERS+32)	/* Not used */
//#define SEND_ALL_JOBS		(SCHED_VERS+33)			/* Not used */
//#define SEND_ALL_JOBS_PRIO	(SCHED_VERS+34)			/* Not used */
#define REQ_NEW_PROC		(SCHED_VERS+35)
#define PCKPT_FRGN_JOB		(SCHED_VERS+36)
//#define SEND_RUNNING_JOBS	(SCHED_VERS+37)			/* Not used */
//#define CHECK_CAPABILITY    (SCHED_VERS+38)		/* Not used */
//#define GIVE_PRIORITY		(SCHED_VERS+39)			/* Not used */
#define	MATCH_INFO			(SCHED_VERS+40)
#define	ALIVE				(SCHED_VERS+41)
#define REQUEST_CLAIM 		(SCHED_VERS+42)
#define RELEASE_CLAIM 		(SCHED_VERS+43)
#define ACTIVATE_CLAIM	 	(SCHED_VERS+44)
//#define PRIORITY_INFO       (SCHED_VERS+45)     /* negotiator to accountant, Not used */
#define PCKPT_ALL_JOBS		(SCHED_VERS+46)
#define VACATE_ALL_CLAIMS	(SCHED_VERS+47)
#define GIVE_STATE			(SCHED_VERS+48)
#define SET_PRIORITY		(SCHED_VERS+49)		// negotiator(priviliged) cmd 
//#define GIVE_CLASSAD		(SCHED_VERS+50)		/* Not used */
#define GET_PRIORITY		(SCHED_VERS+51)		// negotiator
//#define GIVE_REQUEST_AD		(SCHED_VERS+52)		// Starter -> Startd, Not used
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
#define PCKPT_JOB			(SCHED_VERS+66)     // periodic ckpt a given slot
#define DAEMON_OFF			(SCHED_VERS+67)		// specific daemon, subsys follows 
#define DAEMON_OFF_FAST		(SCHED_VERS+68)		// specific daemon, subsys follows 
#define DAEMON_ON			(SCHED_VERS+69)		// specific daemon, subsys follows 
#define GIVE_TOTALS_CLASSAD	(SCHED_VERS+70)
#define DUMP_STATE          (SCHED_VERS+71)	// drop internal vars into classad
#define PERMISSION_AND_AD	(SCHED_VERS+72) // negotiator is sending startad to schedd
//#define REQUEST_NETWORK		(SCHED_VERS+73)	// negotiator network mgmt, Not used
#define VACATE_ALL_FAST		(SCHED_VERS+74)		// fast vacate for whole machine
#define VACATE_CLAIM_FAST	(SCHED_VERS+75)  	// fast vacate for a given slot
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
#define NEGOTIATE_WITH_SIGATTRS	(SCHED_VERS+93)	// pre 7.5.4 NEGOTIATE
#define SET_ACCUMUSAGE	(SCHED_VERS+94)		// negotiator
#define SET_BEGINTIME	(SCHED_VERS+95)		// negotiator
#define SET_LASTTIME	(SCHED_VERS+96)		// negotiator
#define STORE_POOL_CRED		(SCHED_VERS+97)	// master, store password for daemon-to-daemon shared secret auth (PASSWORD)
#define VM_REGISTER	(SCHED_VERS+98)		// Virtual Machine (*not* "slot") ;)
#define DELEGATE_GSI_CRED_SCHEDD	(SCHED_VERS+99) // delegate refreshed gsi proxy to schedd
#define DELEGATE_GSI_CRED_STARTER (SCHED_VERS+100) // delegate refreshed gsi proxy to starter
#define DELEGATE_GSI_CRED_STARTD (SCHED_VERS+101) // delegate gsi proxy to startd
#define REQUEST_SANDBOX_LOCATION (SCHED_VERS+102) // get the sinful of a transferd
#define VM_UNIV_GAHP_ERROR   (SCHED_VERS+103) // report the error of vmgahp to startd
#define VM_UNIV_VMPID		(SCHED_VERS+104) // PID of process for a VM
#define VM_UNIV_GUEST_IP	(SCHED_VERS+105) // IP address of VM
#define VM_UNIV_GUEST_MAC	(SCHED_VERS+106) // MAC address of VM

#define TRANSFER_QUEUE_REQUEST (SCHED_VERS+107) // request to do file transfer

#define SET_SHUTDOWN_PROGRAM (SCHED_VERS+108) // Master: Run program at shutdown
#define GET_JOB_CONNECT_INFO (SCHED_VERS+109) // schedd: get connection information for starter running a job

#define RECYCLE_SHADOW (SCHED_VERS+110) // schedd: get a new job for a shadow
#define CLEAR_DIRTY_JOB_ATTRS (SCHED_VERS+111) // schedd: clear dirty attributes for a job
// These two commands originally used the same command int by mistake.
// In 7.9.6, GET_PRIORITY_ROLLUP was assigned a new command int.
#define GET_PRIORITY_ROLLUP_OLD DRAIN_JOBS // negotiator
#define DRAIN_JOBS (SCHED_VERS+112)
#define CANCEL_DRAIN_JOBS (SCHED_VERS+113)
#define GET_PRIORITY_ROLLUP (SCHED_VERS+114) // negotiator
#define QUERY_SCHEDD_HISTORY (SCHED_VERS+115)
#define QUERY_JOB_ADS (SCHED_VERS+116)
#define SWAP_CLAIM_AND_ACTIVATION (SCHED_VERS+117) // swap claim & activation between two STARTD resources, for moving a job into a 'transfer' slot.
#define SEND_RESOURCE_REQUEST_LIST	(SCHED_VERS+118)     // used in negotiation protocol
#define QUERY_JOB_ADS_WITH_AUTH (SCHED_VERS+119) // Same as QUERY_JOB_ADS but requires authentication
#define FETCH_PROXY_DELEGATION (SCHED_VERS+120)

#define REASSIGN_SLOT (SCHED_VERS+121) // Given two job IDs, deactivate the victim job's claim and reactivate it running the beneficiary job.
#define COALESCE_SLOTS (SCHED_VERS+122) // Given a resource request (job ad) and k claim IDs, invalidate them, merge them into one slot, and return that slot's new claim ID and machine ad.  The resource request is used to compute left-overs.

// Given a token request from a trusted collector, generate an identity token.
#define COLLECTOR_TOKEN_REQUEST (SCHED_VERS+123)
// Get the SubmitterCeiling
#define GET_CEILING (SCHED_VERS+124)
#define SET_CEILING (SCHED_VERS+125)


// values used for "HowFast" in the draining request
#define DRAIN_GRACEFUL 0
#define DRAIN_QUICK 10
#define DRAIN_FAST 20

// values for OnCompletion for draining request
// note that prior to 8.9.11 only Resume and Nothing are recognised
#define DRAIN_NOTHING_ON_COMPLETION 0
#define DRAIN_RESUME_ON_COMPLETION  1
#define DRAIN_EXIT_ON_COMPLETION    2
#define DRAIN_RESTART_ON_COMPLETION 3

// HAD-related commands
#define HAD_ALIVE_CMD                   (HAD_COMMANDS_BASE + 0)
#define HAD_SEND_ID_CMD                 (HAD_COMMANDS_BASE + 1)
//#define HAD_REPL_UPDATE_VERSION         (HAD_COMMANDS_BASE + 2)	/* Not used */
#define HAD_BEFORE_PASSIVE_STATE        (HAD_COMMANDS_BASE + 3)
#define HAD_AFTER_ELECTION_STATE        (HAD_COMMANDS_BASE + 4)
#define HAD_AFTER_LEADER_STATE          (HAD_COMMANDS_BASE + 5)
#define HAD_IN_LEADER_STATE             (HAD_COMMANDS_BASE + 6)

// Replication-related commands
#define REPLICATION_TRANSFER_FILE          (REPLICATION_COMMANDS_BASE + 0)
#define REPLICATION_LEADER_VERSION         (REPLICATION_COMMANDS_BASE + 1)
#define REPLICATION_NEWLY_JOINED_VERSION   (REPLICATION_COMMANDS_BASE + 2)
#define REPLICATION_GIVING_UP_VERSION      (REPLICATION_COMMANDS_BASE + 3)
#define REPLICATION_SOLICIT_VERSION        (REPLICATION_COMMANDS_BASE + 4)
#define REPLICATION_SOLICIT_VERSION_REPLY  (REPLICATION_COMMANDS_BASE + 5)
#define REPLICATION_TRANSFER_FILE_NEW      (REPLICATION_COMMANDS_BASE + 6)

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
#define CA_RENEW_LEASE_FOR_CLAIM (CA_AUTH_CMD_BASE+7)
// other commands that use the ClassAd-only protocol
// CA_LOCATE_STARTER used to be (CA_AUTH_CMD_BASE+7), but no more
// CA_RECONNECT_JOB used to be  (CA_AUTH_CMD_BASE+8), but no more

// Use the ClassAd-based protocol for updating the machine ClassAd.
#define CA_UPDATE_MACHINE_AD	(CA_AUTH_CMD_BASE+9)
// Use the ClassAd-based protocol for communicating with the annex daemon.
#define CA_BULK_REQUEST			(CA_AUTH_CMD_BASE+10)

#define CA_CMD                  (CA_CMD_BASE+0)
#define CA_LOCATE_STARTER       (CA_CMD_BASE+1)
#define CA_RECONNECT_JOB        (CA_CMD_BASE+2)

/* these comments are parsed by command_table_generator.pl
NAMETABLE_DIRECTIVE:BEGIN_SECTION:COLLECTOR
NAMETABLE_DIRECTIVE:PARSE:const int
NAMETABLE_DIRECTIVE:BASE:0
*/

/************
*** Command ids used by the collector 
************/
const int UPDATE_STARTD_AD		= 0;
const int UPDATE_SCHEDD_AD		= 1;
const int UPDATE_MASTER_AD		= 2;
//const int UPDATE_GATEWAY_AD		= 3;			/* Not used */
const int UPDATE_CKPT_SRVR_AD	= 4;

const int QUERY_STARTD_ADS		= 5;
const int QUERY_SCHEDD_ADS		= 6;
const int QUERY_MASTER_ADS		= 7;
//const int QUERY_GATEWAY_ADS		= 8;			/* Not used */
const int QUERY_CKPT_SRVR_ADS	= 9;
const int QUERY_STARTD_PVT_ADS	= 10;

const int UPDATE_SUBMITTOR_AD	= 11;
const int QUERY_SUBMITTOR_ADS	= 12;

const int INVALIDATE_STARTD_ADS	= 13;
const int INVALIDATE_SCHEDD_ADS	= 14;
const int INVALIDATE_MASTER_ADS	= 15;
//const int INVALIDATE_GATEWAY_ADS	= 16;			/* Not used */
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

const int UPDATE_HAD_AD = 55;
const int QUERY_HAD_ADS = 56;
const int INVALIDATE_HAD_ADS = 57;

const int UPDATE_AD_GENERIC = 58;
const int INVALIDATE_ADS_GENERIC = 59;

const int UPDATE_STARTD_AD_WITH_ACK = 60;

//const int UPDATE_XFER_SERVICE_AD		= 61;	/* Not used */
//const int QUERY_XFER_SERVICE_ADS		= 62;	/* Not used */
//const int INVALIDATE_XFER_SERVICE_ADS	= 63;	/* Not used */

//const int UPDATE_LEASE_MANAGER_AD		= 64;	/* Not used */
//const int QUERY_LEASE_MANAGER_ADS		= 65;	/* Not used */
//const int INVALIDATE_LEASE_MANAGER_ADS  = 66;	/* Not used */

const int CCB_REGISTER = 67;
const int CCB_REQUEST = 68;
const int CCB_REVERSE_CONNECT = 69;

const int UPDATE_GRID_AD 	= 70;
const int QUERY_GRID_ADS	= 71;
const int INVALIDATE_GRID_ADS = 72;

const int MERGE_STARTD_AD = 73;
const int QUERY_GENERIC_ADS = 74;

const int SHARED_PORT_CONNECT = 75;
const int SHARED_PORT_PASS_SOCK = 76;

const int UPDATE_ACCOUNTING_AD = 77;
const int QUERY_ACCOUNTING_ADS = 78;
const int INVALIDATE_ACCOUNTING_ADS = 79;

const int UPDATE_OWN_SUBMITTOR_AD = 80;

// Request a collector to retrieve an identity token from a schedd.
const int IMPERSONATION_TOKEN_REQUEST = 81;

/* these comments are used to control command_table_generator.pl
NAMETABLE_DIRECTIVE:END_SECTION:collector
*/

/*
*** Commands to the starter
*/

#define STARTER_COMMANDS_BASE 1500
#define STARTER_HOLD_JOB    (STARTER_COMMANDS_BASE+0)
#define CREATE_JOB_OWNER_SEC_SESSION (STARTER_COMMANDS_BASE+1)
#define START_SSHD (STARTER_COMMANDS_BASE+2)
#define STARTER_PEEK (STARTER_COMMANDS_BASE+3)


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
#define DC_PURGE_LOG        (DC_BASE+18)
//#define DC_SHARE_SOCK       (DC_BASE+19)		/* Not used */

// these are all NOP but registered at different authz levels
#define DC_NOP_READ         (DC_BASE+20)
#define DC_NOP_WRITE        (DC_BASE+21)
#define DC_NOP_NEGOTIATOR   (DC_BASE+22)
#define DC_NOP_ADMINISTRATOR (DC_BASE+23)
#define DC_NOP_OWNER        (DC_BASE+24)
#define DC_NOP_CONFIG       (DC_BASE+25)
#define DC_NOP_DAEMON       (DC_BASE+26)
#define DC_NOP_ADVERTISE_STARTD (DC_BASE+27)
#define DC_NOP_ADVERTISE_SCHEDD (DC_BASE+28)
#define DC_NOP_ADVERTISE_MASTER (DC_BASE+29)
// leave 30-39 open -- zmiller
#define DC_SEC_QUERY        (DC_BASE+40)
#define DC_SET_FORCE_SHUTDOWN (DC_BASE+41)
#define DC_OFF_FORCE       (DC_BASE+42)
#define DC_SET_READY       (DC_BASE+43)  // sent to parent to indicate a demon is ready for use
#define DC_QUERY_READY     (DC_BASE+44)  // daemon command handler should reply only once it and children are ready
#define DC_QUERY_INSTANCE  (DC_BASE+45)  // ask if a daemon is alive - returns a random 64 bit int that will not change as long as this instance is alive.
#define DC_GET_SESSION_TOKEN (DC_BASE+46) // Retrieve an authentication token for TOKEN that is at most equivalent to the current session.
#define DC_START_TOKEN_REQUEST (DC_BASE+47) // Request a token from this daemon.
#define DC_FINISH_TOKEN_REQUEST (DC_BASE+48) // Poll remote daemon for available token.
#define DC_LIST_TOKEN_REQUEST (DC_BASE+49) // Poll for the existing token requests.
#define DC_APPROVE_TOKEN_REQUEST (DC_BASE+50) // Approve a token request.
#define DC_AUTO_APPROVE_TOKEN_REQUEST (DC_BASE+51) // Auto-approve token requests.
#define DC_EXCHANGE_SCITOKEN (DC_BASE+52) // Exchange a SciToken for a Condor token.

/*
*** Log type supported by DC_FETCH_LOG
*** These are not interpreted directly by DaemonCore,
*** so it's ok that they start at zero.
*/

#define DC_FETCH_LOG_TYPE_PLAIN 0
#define DC_FETCH_LOG_TYPE_HISTORY 1
#define DC_FETCH_LOG_TYPE_HISTORY_DIR 2
#define DC_FETCH_LOG_TYPE_HISTORY_PURGE 3
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
//#define PW_BASE 70000							/* Not used */
//#define PW_SETPASS			(PW_BASE+1)		/* Not used */
//#define PW_GETPASS			(PW_BASE+2)		/* Not used */
//#define PW_CLEARPASS		(PW_BASE+3)			/* Not used */

/*
*** Commands used by the daemon core Shadow
*/
#define DCSHADOW_BASE 71000
#define SHADOW_UPDATEINFO	   (DCSHADOW_BASE+0)
//#define TAKE_MATCH             (DCSHADOW_BASE+1)  // for MPI & parallel shadow, Not used
#define MPI_START_COMRADE      (DCSHADOW_BASE+2)  // for MPI & parallel shadow
#define GIVE_MATCHES 	       (DCSHADOW_BASE+3)  // for MPI & parallel shadow
//#define RECEIVE_JOBAD		   (DCSHADOW_BASE+4)	/* Not used */
#define UPDATE_JOBAD		   (DCSHADOW_BASE+5)


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
*** Commands used by the transfer daemon
*/
#define TRANSFERD_BASE 74000
/* This is used by the schedd when a transferd registers itself */
#define TRANSFERD_REGISTER		(TRANSFERD_BASE+0)
/* a channel under which a transferd may be sent control message such as
	being informed of a new a new transfer request 
*/
#define TRANSFERD_CONTROL_CHANNEL	(TRANSFERD_BASE+1)
/* Files are being written to the transferd for storage */
#define TRANSFERD_WRITE_FILES	(TRANSFERD_BASE+2)
/* files are being read from the transferd's storage */
#define TRANSFERD_READ_FILES	(TRANSFERD_BASE+3)


/*
*** Commands used by the credd daemon
*/
#define CREDD_BASE 81000	
#define CREDD_STORE_CRED (CREDD_BASE+0)
#define CREDD_GET_CRED (CREDD_BASE+1)
#define CREDD_REMOVE_CRED (CREDD_BASE+2)
#define CREDD_QUERY_CRED (CREDD_BASE+3)
#define CREDD_REFRESH_ALL (CREDD_BASE+20)
#define CREDD_CHECK_CREDS (CREDD_BASE+30)	// check to see if the desired oauth tokens are stored
#define CREDD_GET_PASSWD (CREDD_BASE+99)	// used by the Win32 credd only
#define CREDD_NOP (CREDD_BASE+100)			// used by the Win32 credd only


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

/* Replies specific to the REQUEST_CLAIM command */
#define REQUEST_CLAIM_LEFTOVERS		3
#define REQUEST_CLAIM_PAIR			4
#define REQUEST_CLAIM_LEFTOVERS_2	5
#define REQUEST_CLAIM_PAIR_2		6

/* Replies specific to the SWAP_CLAIM_AND_ACTIVATION command */
#define SWAP_CLAIM_ALREADY_SWAPPED	4

#endif  /* of ifndef _CONDOR_COMMANDS_H */
