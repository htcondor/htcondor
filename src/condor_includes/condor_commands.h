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

#include <array>
#include <climits>

/*
*** Replies used in various stages of various protocols
*/

/* Failure cases */
const int NOT_OK		= 0;

/* Success cases */
const int OK			= 1;

/* Other replies */
const int CONDOR_TRY_AGAIN	= 2;
const int CONDOR_ERROR	    = 3;

/* Replies specific to the REQUEST_CLAIM command */
const int REQUEST_CLAIM_LEFTOVERS        = 3;
//const int REQUEST_CLAIM_PAIR           = 4;   // Not used
const int REQUEST_CLAIM_LEFTOVERS_2	     = 5;
//const int REQUEST_CLAIM_PAIR_2         = 6;   // Not used
const int REQUEST_CLAIM_SLOT_AD          = 7;


// The contraints on this file are
// 1) We want a #define'd symbol for each command for client code to use
// 2) We don't want to pollute every .o's data segment with a copy of a table
// 3) We rarely look up a command with a string or command int known at compile-time
// 		so we don't need a constexpr function do to so
// 4) The table of mappings between command int and string is big and not compact
// 		So magic-enum tricks lead to very slow compile times
// 		And we're willing to binary search at runtime to do the lookup
//
// 	So ...
// 	We define a constexpr function in this header file that returns a 
// 	constexpr std::array that can be constexpr sorted by either field
// 	in the Get..Num or Get..String functions, so that they can binary
// 	search that sorted table.  This constexpr table-generating funciton
// 	is in this header file, so that we can put the #define's in the
// 	same line as what generates the tables -- many clients need the
// 	#defines, but only condor_command.cpp needs the resulting std::arry,
// 	so the array will only show up in the condor_commands.o object file
// 	#define for each command, conf


constexpr const
std::array<std::pair<int, const char *>, 199> makeCommandTable() {
	return {{ // Yes, we need two...

/****
** Queue Manager Commands
****/
#define QMGMT_BASE		1110
#define QMGMT_READ_CMD	(QMGMT_BASE+1)
		{QMGMT_READ_CMD, "QMGMT_READ_CMD"},
#define QMGMT_WRITE_CMD	(QMGMT_BASE+2)
		{QMGMT_WRITE_CMD, "QMGMT_WRITE_CMD"},

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
		{CONTINUE_CLAIM, "CONTINUE_CLAIM"},
#define SUSPEND_CLAIM		(SCHED_VERS+2)		// New name for SUSPEND_FRGN_JOB
		{SUSPEND_CLAIM, "SUSPEND_CLAIM"},
#define DEACTIVATE_CLAIM	(SCHED_VERS+3)		// New name for CKPT_FRGN_JOB
		{DEACTIVATE_CLAIM, "DEACTIVATE_CLAIM"},
#define DEACTIVATE_CLAIM_FORCIBLY	(SCHED_VERS+4)		// New name for KILL_FRGN_JOB
		{DEACTIVATE_CLAIM_FORCIBLY, "DEACTIVATE_CLAIM_FORCIBLY"},

#define LOCAL_STATUS		(SCHED_VERS+5)		/* Not used */
		{LOCAL_STATUS, "LOCAL_STATUS"},
//#define LOCAL_STATISTICS	(SCHED_VERS+6)		/* Not used */
//		{LOCAL_STATISTICS, "LOCAL_STATISTICS"},
#define PERMISSION			(SCHED_VERS+7)		// used in negotiation protocol
		{PERMISSION, "PERMISSION"},
//#define SET_DEBUG_FLAGS		(SCHED_VERS+8)		/* Not used */
//		{SET_DEBUG_FLAGS, "SET_DEBUG_FLAGS"},
//#define PREEMPT_LOCAL_JOBS	(SCHED_VERS+9)		/* Not used */
//		{PREEMPT_LOCAL_JOBS, "PREEMPT_LOCAL_JOBS"},

//#define RM_LOCAL_JOB		(SCHED_VERS+10)		/* Not used */
//		{RM_LOCAL_JOB, "RM_LOCAL_JOB"},
//#define START_FRGN_JOB		(SCHED_VERS+11)		/* Not used */
//		{START_FRGN_JOB, "START_FRGN_JOB"},

//#define AVAILABILITY		(SCHED_VERS+12)		/* Not used */
//		{AVAILABILITY, "AVAILABILITY"},
//#define NUM_FRGN_JOBS		(SCHED_VERS+13)		/* Not used */
//		{NUM_FRGN_JOBS, "NUM_FRGN_JOBS"},
//#define STARTD_INFO			(SCHED_VERS+14)		/* Not used */
//		{STARTD_INFO, "STARTD_INFO"},
//#define SCHEDD_INFO			(SCHED_VERS+15)		/* Not used */
//		{SCHEDD_INFO, "SCHEDD_INFO"},
#define NEGOTIATE			(SCHED_VERS+16) // 7.5.4+ negotiation command
		{NEGOTIATE, "NEGOTIATE"},
#define SEND_JOB_INFO		(SCHED_VERS+17)     // used in negotiation protocol
		{SEND_JOB_INFO, "SEND_JOB_INFO"},
#define NO_MORE_JOBS		(SCHED_VERS+18)		// used in negotiation protocol
		{NO_MORE_JOBS, "NO_MORE_JOBS"},
#define JOB_INFO			(SCHED_VERS+19)		// used in negotiation protocol
		{JOB_INFO, "JOB_INFO"},
//#define GIVE_STATUS			(SCHED_VERS+20)		/* Not used */
//		{GIVE_STATUS, "GIVE_STATUS"},
#define RESCHEDULE			(SCHED_VERS+21)
		{RESCHEDULE, "RESCHEDULE"},
//#define PING				(SCHED_VERS+22)			/* Not used */
//		{PING, "PING"},
//#define NEGOTIATOR_INFO		(SCHED_VERS+23)		/* Not used */
//		{NEGOTIATOR_INFO, "NEGOTIATOR_INFO"},
//#define GIVE_STATUS_LINES	(SCHED_VERS+24)			/* Not used */
//		{GIVE_STATUS_LINES, "GIVE_STATUS_LINES"},
#define END_NEGOTIATE		(SCHED_VERS+25)		// used in negotiation protocol
		{END_NEGOTIATE, "END_NEGOTIATE"},
#define REJECTED			(SCHED_VERS+26)
		{REJECTED, "REJECTED"},
#define X_EVENT_NOTIFICATION		(SCHED_VERS+27)
		{X_EVENT_NOTIFICATION, "X_EVENT_NOTIFICATION"},
//#define RECONFIG			(SCHED_VERS+28)			/* Not used */
//		{RECONFIG, "RECONFIG"},
#define GET_HISTORY			(SCHED_VERS+29)		/* repurposed in 8.9.7 to be query startd history */
		{GET_HISTORY, "GET_HISTORY"},
//#define UNLINK_HISTORY_FILE			(SCHED_VERS+30)	/* Not used */
//		{UNLINK_HISTORY_FILE, "UNLINK_HISTORY_FILE"},
//#define UNLINK_HISTORY_FILE_DONE	(SCHED_VERS+31)		/* Not used */
//		{UNLINK_HISTORY_FILE_DONE, "UNLINK_HISTORY_FILE_DONE"},
//#define DO_NOT_UNLINK_HISTORY_FILE	(SCHED_VERS+32)	/* Not used */
//		{DO_NOT_UNLINK_HISTORY_FILE, "DO_NOT_UNLINK_HISTORY_FILE"},
//#define SEND_ALL_JOBS		(SCHED_VERS+33)			/* Not used */
//		{SEND_ALL_JOBS, "SEND_ALL_JOBS"},
//#define SEND_ALL_JOBS_PRIO	(SCHED_VERS+34)			/* Not used */
//		{SEND_ALL_JOBS_PRIO, "SEND_ALL_JOBS_PRIO"},
#define REQ_NEW_PROC		(SCHED_VERS+35)
		{REQ_NEW_PROC, "REQ_NEW_PROC"},
#define PCKPT_FRGN_JOB		(SCHED_VERS+36)
		{PCKPT_FRGN_JOB, "PCKPT_FRGN_JOB"},
//#define SEND_RUNNING_JOBS	(SCHED_VERS+37)			/* Not used */
//		{SEND_RUNNING_JOBS, "SEND_RUNNING_JOBS"},
//#define CHECK_CAPABILITY    (SCHED_VERS+38)		/* Not used */
//		{CHECK_CAPABILITY, "CHECK_CAPABILITY"},
//#define GIVE_PRIORITY		(SCHED_VERS+39)			/* Not used */
//		{GIVE_PRIORITY, "GIVE_PRIORITY"},
#define	MATCH_INFO			(SCHED_VERS+40)
		{MATCH_INFO, "MATCH_INFO"},
#define	ALIVE				(SCHED_VERS+41)
		{ALIVE, "ALIVE"},
#define REQUEST_CLAIM 		(SCHED_VERS+42)
		{REQUEST_CLAIM, "REQUEST_CLAIM"},
#define RELEASE_CLAIM 		(SCHED_VERS+43)
		{RELEASE_CLAIM, "RELEASE_CLAIM"},
#define ACTIVATE_CLAIM	 	(SCHED_VERS+44)
		{ACTIVATE_CLAIM, "ACTIVATE_CLAIM"},
//#define PRIORITY_INFO       (SCHED_VERS+45)     /* negotiator to accountant, Not used */
//		{PRIORITY_INFO, "PRIORITY_INFO"},
#define PCKPT_ALL_JOBS		(SCHED_VERS+46)
		{PCKPT_ALL_JOBS, "PCKPT_ALL_JOBS"},
#define VACATE_ALL_CLAIMS	(SCHED_VERS+47)
		{VACATE_ALL_CLAIMS, "VACATE_ALL_CLAIMS"},
#define GIVE_STATE			(SCHED_VERS+48)
		{GIVE_STATE, "GIVE_STATE"},
#define SET_PRIORITY		(SCHED_VERS+49)		// negotiator(priviliged) cmd
		{SET_PRIORITY, "SET_PRIORITY"},
//#define GIVE_CLASSAD		(SCHED_VERS+50)		/* Not used */
//		{GIVE_CLASSAD, "GIVE_CLASSAD"},
#define GET_PRIORITY		(SCHED_VERS+51)		// negotiator
		{GET_PRIORITY, "GET_PRIORITY"},
#define DAEMONS_OFF_FLEX	(SCHED_VERS+52)		// DAEMON_OFF but with a classad payload and optional reply
		{DAEMONS_OFF_FLEX, "DAEMONS_OFF_FLEX"},
#define RESTART				(SCHED_VERS+53)
		{RESTART, "RESTART"},
#define DAEMONS_OFF			(SCHED_VERS+54)
		{DAEMONS_OFF, "DAEMONS_OFF"},
#define DAEMONS_ON			(SCHED_VERS+55)
		{DAEMONS_ON, "DAEMONS_ON"},
#define MASTER_OFF			(SCHED_VERS+56)
		{MASTER_OFF, "MASTER_OFF"},
#define CONFIG_VAL			(SCHED_VERS+57)
		{CONFIG_VAL, "CONFIG_VAL"},
#define RESET_USAGE			(SCHED_VERS+58)		// negotiator
		{RESET_USAGE, "RESET_USAGE"},
#define SET_PRIORITYFACTOR	(SCHED_VERS+59)		// negotiator
		{SET_PRIORITYFACTOR, "SET_PRIORITYFACTOR"},
#define RESET_ALL_USAGE		(SCHED_VERS+60)		// negotiator
		{RESET_ALL_USAGE, "RESET_ALL_USAGE"},
#define DAEMONS_OFF_FAST	(SCHED_VERS+61)
		{DAEMONS_OFF_FAST, "DAEMONS_OFF_FAST"},
#define MASTER_OFF_FAST		(SCHED_VERS+62)
		{MASTER_OFF_FAST, "MASTER_OFF_FAST"},
#define GET_RESLIST			(SCHED_VERS+63)		// negotiator
		{GET_RESLIST, "GET_RESLIST"},
#define ATTEMPT_ACCESS		(SCHED_VERS+64) 	// schedd, test a file
		{ATTEMPT_ACCESS, "ATTEMPT_ACCESS"},
#define VACATE_CLAIM		(SCHED_VERS+65)     // vacate a given claim
		{VACATE_CLAIM, "VACATE_CLAIM"},
#define PCKPT_JOB			(SCHED_VERS+66)     // periodic ckpt a given slot
		{PCKPT_JOB, "PCKPT_JOB"},
#define DAEMON_OFF			(SCHED_VERS+67)		// specific daemon, subsys follows
		{DAEMON_OFF, "DAEMON_OFF"},
#define DAEMON_OFF_FAST		(SCHED_VERS+68)		// specific daemon, subsys follows
		{DAEMON_OFF_FAST, "DAEMON_OFF_FAST"},
#define DAEMON_ON			(SCHED_VERS+69)		// specific daemon, subsys follows
		{DAEMON_ON, "DAEMON_ON"},
#define GIVE_TOTALS_CLASSAD	(SCHED_VERS+70)
		{GIVE_TOTALS_CLASSAD, "GIVE_TOTALS_CLASSAD"},
//#define DUMP_STATE          (SCHED_VERS+71)	// drop internal vars into classad (Not used)
//		{DUMP_STATE, "DUMP_STATE"},
#define PERMISSION_AND_AD	(SCHED_VERS+72) // negotiator is sending startad to schedd
		{PERMISSION_AND_AD, "PERMISSION_AND_AD"},
//#define REQUEST_NETWORK		(SCHED_VERS+73)	// negotiator network mgmt, Not used
//		{REQUEST_NETWORK, "REQUEST_NETWORK"},
#define VACATE_ALL_FAST		(SCHED_VERS+74)		// fast vacate for whole machine
		{VACATE_ALL_FAST, "VACATE_ALL_FAST"},
#define VACATE_CLAIM_FAST	(SCHED_VERS+75)  	// fast vacate for a given slot
		{VACATE_CLAIM_FAST, "VACATE_CLAIM_FAST"},
#define REJECTED_WITH_REASON (SCHED_VERS+76) // diagnostic version of REJECTED
		{REJECTED_WITH_REASON, "REJECTED_WITH_REASON"},
#define START_AGENT			(SCHED_VERS+77) // have the master start an agenta (Not used)
		{START_AGENT, "START_AGENT"},
#define ACT_ON_JOBS			(SCHED_VERS+78) // have the schedd act on some jobs (rm, hold, release)
		{ACT_ON_JOBS, "ACT_ON_JOBS"},
#define STORE_CRED			(SCHED_VERS+79)		// schedd, store a credential
		{STORE_CRED, "STORE_CRED"},
#define SPOOL_JOB_FILES		(SCHED_VERS+80)	// spool all job files via filetransfer object
		{SPOOL_JOB_FILES, "SPOOL_JOB_FILES"},
//#define GET_MYPROXY_PASSWORD (SCHED_VERS+81) // gmanager->schedd: Give me MyProxy password, Not used
//		{GET_MYPROXY_PASSWORD, "GET_MYPROXY_PASSWORD"},
#define DELETE_USER			(SCHED_VERS+82)		// negotiator  (actually, accountant)
		{DELETE_USER, "DELETE_USER"},
#define DAEMON_OFF_PEACEFUL  (SCHED_VERS+83)		// specific daemon, subsys follows
		{DAEMON_OFF_PEACEFUL, "DAEMON_OFF_PEACEFUL"},
#define DAEMONS_OFF_PEACEFUL (SCHED_VERS+84)
		{DAEMONS_OFF_PEACEFUL, "DAEMONS_OFF_PEACEFUL"},
#define RESTART_PEACEFUL     (SCHED_VERS+85)
		{RESTART_PEACEFUL, "RESTART_PEACEFUL"},
#define TRANSFER_DATA		(SCHED_VERS+86) // send all job files back via filetransfer object
		{TRANSFER_DATA, "TRANSFER_DATA"},
#define UPDATE_GSI_CRED		(SCHED_VERS+87) // send refreshed gsi proxy file
		{UPDATE_GSI_CRED, "UPDATE_GSI_CRED"},
#define SPOOL_JOB_FILES_WITH_PERMS	(SCHED_VERS+88)	// spool all job files via filetransfer object (new version with file permissions)
		{SPOOL_JOB_FILES_WITH_PERMS, "SPOOL_JOB_FILES_WITH_PERMS"},
#define TRANSFER_DATA_WITH_PERMS	(SCHED_VERS+89) // send all job files back via filetransfer object (new version with file permissions)
		{TRANSFER_DATA_WITH_PERMS, "TRANSFER_DATA_WITH_PERMS"},
#define CHILD_ON            (SCHED_VERS+90) // Turn my child ON (HAD)
		{CHILD_ON, "CHILD_ON"},
#define CHILD_OFF           (SCHED_VERS+91) // Turn my child OFF (HAD)
		{CHILD_OFF, "CHILD_OFF"},
#define CHILD_OFF_FAST      (SCHED_VERS+92) // Turn my child OFF/Fast (HAD)
		{CHILD_OFF_FAST, "CHILD_OFF_FAST"},
//#define NEGOTIATE_WITH_SIGATTRS	(SCHED_VERS+93)	// pre 7.5.4 NEGOTIATE (unused)
//		{NEGOTIATE_WITH_SIGATTRS, "NEGOTIATE_WITH_SIGATTRS"},
#define SET_ACCUMUSAGE	(SCHED_VERS+94)		// negotiator
		{SET_ACCUMUSAGE, "SET_ACCUMUSAGE"},
#define SET_BEGINTIME	(SCHED_VERS+95)		// negotiator
		{SET_BEGINTIME, "SET_BEGINTIME"},
#define SET_LASTTIME	(SCHED_VERS+96)		// negotiator
		{SET_LASTTIME, "SET_LASTTIME"},
#define STORE_POOL_CRED		(SCHED_VERS+97)	// master, store password for daemon-to-daemon shared secret auth (PASSWORD)
		{STORE_POOL_CRED, "STORE_POOL_CRED"},
//#define VM_REGISTER	(SCHED_VERS+98)		// Virtual Machine (*not* "slot") (Not used)
//		{VM_REGISTER, "VM_REGISTER"},
#define DELEGATE_GSI_CRED_SCHEDD	(SCHED_VERS+99) // delegate refreshed gsi proxy to schedd
		{DELEGATE_GSI_CRED_SCHEDD, "DELEGATE_GSI_CRED_SCHEDD"},
#define DELEGATE_GSI_CRED_STARTER (SCHED_VERS+100) // delegate refreshed gsi proxy to starter
		{DELEGATE_GSI_CRED_STARTER, "DELEGATE_GSI_CRED_STARTER"},
#define DELEGATE_GSI_CRED_STARTD (SCHED_VERS+101) // delegate gsi proxy to startd
		{DELEGATE_GSI_CRED_STARTD, "DELEGATE_GSI_CRED_STARTD"},
#define REQUEST_SANDBOX_LOCATION (SCHED_VERS+102) // get the sinful of a transferd (Not used)
		{REQUEST_SANDBOX_LOCATION, "REQUEST_SANDBOX_LOCATION"},
#define VM_UNIV_GAHP_ERROR   (SCHED_VERS+103) // report the error of vmgahp to startd
		{VM_UNIV_GAHP_ERROR, "VM_UNIV_GAHP_ERROR"},
#define VM_UNIV_VMPID		(SCHED_VERS+104) // PID of process for a VM
		{VM_UNIV_VMPID, "VM_UNIV_VMPID"},
#define VM_UNIV_GUEST_IP	(SCHED_VERS+105) // IP address of VM
		{VM_UNIV_GUEST_IP, "VM_UNIV_GUEST_IP"},
#define VM_UNIV_GUEST_MAC	(SCHED_VERS+106) // MAC address of VM
		{VM_UNIV_GUEST_MAC, "VM_UNIV_GUEST_MAC"},

#define TRANSFER_QUEUE_REQUEST (SCHED_VERS+107) // request to do file transfer
		{TRANSFER_QUEUE_REQUEST, "TRANSFER_QUEUE_REQUEST"},

#define SET_SHUTDOWN_PROGRAM (SCHED_VERS+108) // Master: Run program at shutdown
		{SET_SHUTDOWN_PROGRAM, "SET_SHUTDOWN_PROGRAM"},
#define GET_JOB_CONNECT_INFO (SCHED_VERS+109) // schedd: get connection information for starter running a job
		{GET_JOB_CONNECT_INFO, "GET_JOB_CONNECT_INFO"},

#define RECYCLE_SHADOW (SCHED_VERS+110) // schedd: get a new job for a shadow
		{RECYCLE_SHADOW, "RECYCLE_SHADOW"},
#define CLEAR_DIRTY_JOB_ATTRS (SCHED_VERS+111) // schedd: clear dirty attributes for a job
		{CLEAR_DIRTY_JOB_ATTRS, "CLEAR_DIRTY_JOB_ATTRS"},
		// These two commands originally used the same command int by mistake.
		// In 7.9.6, GET_PRIORITY_ROLLUP was assigned a new command int.
		// List DRAIN_JOBS first, so that lower_bound finds it first
#define DRAIN_JOBS (SCHED_VERS+112)
		{DRAIN_JOBS, "DRAIN_JOBS"},
#define GET_PRIORITY_ROLLUP_OLD (SCHED_VERS+112) // negotiator
		{GET_PRIORITY_ROLLUP_OLD, "GET_PRIORITY_ROLLUP_OLD"},
#define CANCEL_DRAIN_JOBS (SCHED_VERS+113)
		{CANCEL_DRAIN_JOBS, "CANCEL_DRAIN_JOBS"},
#define GET_PRIORITY_ROLLUP (SCHED_VERS+114) // negotiator
		{GET_PRIORITY_ROLLUP, "GET_PRIORITY_ROLLUP"},
#define QUERY_SCHEDD_HISTORY (SCHED_VERS+115)
		{QUERY_SCHEDD_HISTORY, "QUERY_SCHEDD_HISTORY"},
#define QUERY_JOB_ADS (SCHED_VERS+116)
		{QUERY_JOB_ADS, "QUERY_JOB_ADS"},
//#define SWAP_CLAIM_AND_ACTIVATION (SCHED_VERS+117) // swap claim & activation between two STARTD resources, for moving a job into a 'transfer' slot. (Not used)
//		{SWAP_CLAIM_AND_ACTIVATION, "SWAP_CLAIM_AND_ACTIVATION"},
#define SEND_RESOURCE_REQUEST_LIST	(SCHED_VERS+118)     // used in negotiation protocol
		{SEND_RESOURCE_REQUEST_LIST, "SEND_RESOURCE_REQUEST_LIST"},
#define QUERY_JOB_ADS_WITH_AUTH (SCHED_VERS+119) // Same as QUERY_JOB_ADS but requires authentication
		{QUERY_JOB_ADS_WITH_AUTH, "QUERY_JOB_ADS_WITH_AUTH"},
#define FETCH_PROXY_DELEGATION (SCHED_VERS+120)
		{FETCH_PROXY_DELEGATION, "FETCH_PROXY_DELEGATION"},

#define REASSIGN_SLOT (SCHED_VERS+121) // Given two job IDs, deactivate the victim job's claim and reactivate it running the beneficiary job.
		{REASSIGN_SLOT, "REASSIGN_SLOT"},
#define COALESCE_SLOTS (SCHED_VERS+122) // Given a resource request (job ad) and k claim IDs, invalidate them, merge them into one slot, and return that slot's new claim ID and machine ad.  The resource request is used to compute left-overs.
		{COALESCE_SLOTS, "COALESCE_SLOTS"},

		// Given a token request from a trusted collector, generate an identity token.
#define COLLECTOR_TOKEN_REQUEST (SCHED_VERS+123)
		{COLLECTOR_TOKEN_REQUEST, "COLLECTOR_TOKEN_REQUEST"},
		// Get the SubmitterCeiling
#define GET_CEILING (SCHED_VERS+124)
		{GET_CEILING, "GET_CEILING"},
#define SET_CEILING (SCHED_VERS+125)
		{SET_CEILING, "SET_CEILING"},

#define EXPORT_JOBS (SCHED_VERS+126) // Schedd: export selected jobs to a new job_queue.log put jobs into externally managed state
		{EXPORT_JOBS, "EXPORT_JOBS"},
#define IMPORT_EXPORTED_JOB_RESULTS (SCHED_VERS+127) // Schedd: import changes to previously exported jobs and take them out of managed state
		{IMPORT_EXPORTED_JOB_RESULTS, "IMPORT_EXPORTED_JOB_RESULTS"},
#define UNEXPORT_JOBS (SCHED_VERS+128) // Schedd: undo previous export of selected jobs, taking them out of managed state
		{UNEXPORT_JOBS, "UNEXPORT_JOBS"},
		// Get the Submitter Floor
#define GET_FLOOR (SCHED_VERS+129)
		{GET_FLOOR, "GET_FLOOR"},
#define SET_FLOOR (SCHED_VERS+130)
		{SET_FLOOR, "SET_FLOOR"},
#define DIRECT_ATTACH (SCHED_VERS+131) // Provide slot ads to the schedd (not from the negotiator)
		{DIRECT_ATTACH, "DIRECT_ATTACH"},
// command ids from +140 to +149 reserved for Schedd UserRec commands
#define QUERY_USERREC_ADS (SCHED_VERS+140)
		{QUERY_USERREC_ADS, "QUERY_USERREC_ADS"},
#define ENABLE_USERREC    (SCHED_VERS+141) // enbable is also add
		{ENABLE_USERREC, "ENABLE_USERREC"},
#define DISABLE_USERREC   (SCHED_VERS+142)
		{DISABLE_USERREC, "DISABLE_USERREC"},
#define EDIT_USERREC      (SCHED_VERS+143)
		{EDIT_USERREC, "EDIT_USERREC"},
#define RESET_USERREC     (SCHED_VERS+144)
		{RESET_USERREC, "RESET_USERREC"},
#define DELETE_USERREC    (SCHED_VERS+149)
		{DELETE_USERREC, "DELETE_USERREC"},
#define USER_LOGIN    (SCHED_VERS+150)
		{USER_LOGIN, "USER_LOGIN"},
#define MAP_USER    (SCHED_VERS+151)
		{MAP_USER, "MAP_USER"},

#define HAD_ALIVE_CMD                   (HAD_COMMANDS_BASE + 0)
		{HAD_ALIVE_CMD, "HAD_ALIVE_CMD"},
#define HAD_SEND_ID_CMD                 (HAD_COMMANDS_BASE + 1)
		{HAD_SEND_ID_CMD, "HAD_SEND_ID_CMD"},
//#define HAD_REPL_UPDATE_VERSION         (HAD_COMMANDS_BASE + 2)	/* Not used */
//		{HAD_REPL_UPDATE_VERSION, "HAD_REPL_UPDATE_VERSION"},
#define HAD_BEFORE_PASSIVE_STATE        (HAD_COMMANDS_BASE + 3)
		{HAD_BEFORE_PASSIVE_STATE, "HAD_BEFORE_PASSIVE_STATE"},
#define HAD_AFTER_ELECTION_STATE        (HAD_COMMANDS_BASE + 4)
		{HAD_AFTER_ELECTION_STATE, "HAD_AFTER_ELECTION_STATE"},
#define HAD_AFTER_LEADER_STATE          (HAD_COMMANDS_BASE + 5)
		{HAD_AFTER_LEADER_STATE, "HAD_AFTER_LEADER_STATE"},
#define HAD_IN_LEADER_STATE             (HAD_COMMANDS_BASE + 6)
		{HAD_IN_LEADER_STATE, "HAD_IN_LEADER_STATE"},

		// Replication-related commands
#define REPLICATION_TRANSFER_FILE          (REPLICATION_COMMANDS_BASE + 0)
		{REPLICATION_TRANSFER_FILE, "REPLICATION_TRANSFER_FILE"},
#define REPLICATION_LEADER_VERSION         (REPLICATION_COMMANDS_BASE + 1)
		{REPLICATION_LEADER_VERSION, "REPLICATION_LEADER_VERSION"},
#define REPLICATION_NEWLY_JOINED_VERSION   (REPLICATION_COMMANDS_BASE + 2)
		{REPLICATION_NEWLY_JOINED_VERSION, "REPLICATION_NEWLY_JOINED_VERSION"},
#define REPLICATION_GIVING_UP_VERSION      (REPLICATION_COMMANDS_BASE + 3)
		{REPLICATION_GIVING_UP_VERSION, "REPLICATION_GIVING_UP_VERSION"},
#define REPLICATION_SOLICIT_VERSION        (REPLICATION_COMMANDS_BASE + 4)
		{REPLICATION_SOLICIT_VERSION, "REPLICATION_SOLICIT_VERSION"},
#define REPLICATION_SOLICIT_VERSION_REPLY  (REPLICATION_COMMANDS_BASE + 5)
		{REPLICATION_SOLICIT_VERSION_REPLY, "REPLICATION_SOLICIT_VERSION_REPLY"},
#define REPLICATION_TRANSFER_FILE_NEW      (REPLICATION_COMMANDS_BASE + 6)
		{REPLICATION_TRANSFER_FILE_NEW, "REPLICATION_TRANSFER_FILE_NEW"},

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
		{CA_AUTH_CMD, "CA_AUTH_CMD"},

		// generic claiming protocol that the startd uses for COD
#define CA_REQUEST_CLAIM        (CA_AUTH_CMD_BASE+1)
		{CA_REQUEST_CLAIM, "CA_REQUEST_CLAIM"},
#define CA_RELEASE_CLAIM        (CA_AUTH_CMD_BASE+2)
		{CA_RELEASE_CLAIM, "CA_RELEASE_CLAIM"},
#define CA_ACTIVATE_CLAIM       (CA_AUTH_CMD_BASE+3)
		{CA_ACTIVATE_CLAIM, "CA_ACTIVATE_CLAIM"},
#define CA_DEACTIVATE_CLAIM     (CA_AUTH_CMD_BASE+4)
		{CA_DEACTIVATE_CLAIM, "CA_DEACTIVATE_CLAIM"},
#define CA_SUSPEND_CLAIM        (CA_AUTH_CMD_BASE+5)
		{CA_SUSPEND_CLAIM, "CA_SUSPEND_CLAIM"},
#define CA_RESUME_CLAIM         (CA_AUTH_CMD_BASE+6)
		{CA_RESUME_CLAIM, "CA_RESUME_CLAIM"},
#define CA_RENEW_LEASE_FOR_CLAIM (CA_AUTH_CMD_BASE+7)
		{CA_RENEW_LEASE_FOR_CLAIM, "CA_RENEW_LEASE_FOR_CLAIM"},
		// other commands that use the ClassAd-only protocol
		// CA_LOCATE_STARTER used to be (CA_AUTH_CMD_BASE+7), but no more
		// CA_RECONNECT_JOB used to be  (CA_AUTH_CMD_BASE+8), but no more

		// Use the ClassAd-based protocol for updating the machine ClassAd.
#define CA_UPDATE_MACHINE_AD	(CA_AUTH_CMD_BASE+9)
		{CA_UPDATE_MACHINE_AD, "CA_UPDATE_MACHINE_AD"},
		// Use the ClassAd-based protocol for communicating with the annex daemon.
#define CA_BULK_REQUEST			(CA_AUTH_CMD_BASE+10)
		{CA_BULK_REQUEST, "CA_BULK_REQUEST"},

#define CA_CMD                  (CA_CMD_BASE+0)
		{CA_CMD, "CA_CMD"},
#define CA_LOCATE_STARTER       (CA_CMD_BASE+1)
		{CA_LOCATE_STARTER, "CA_LOCATE_STARTER"},
#define CA_RECONNECT_JOB        (CA_CMD_BASE+2)
		{CA_RECONNECT_JOB, "CA_RECONNECT_JOB"},
/*
*** Commands to the starter
*/

#define STARTER_COMMANDS_BASE 1500
#define STARTER_HOLD_JOB    (STARTER_COMMANDS_BASE+0)
		{STARTER_HOLD_JOB, "STARTER_HOLD_JOB"},
#define CREATE_JOB_OWNER_SEC_SESSION (STARTER_COMMANDS_BASE+1)
		{CREATE_JOB_OWNER_SEC_SESSION, "CREATE_JOB_OWNER_SEC_SESSION"},
#define START_SSHD (STARTER_COMMANDS_BASE+2)
		{START_SSHD, "START_SSHD"},
#define STARTER_PEEK (STARTER_COMMANDS_BASE+3)
		{STARTER_PEEK, "STARTER_PEEK"},

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
		{DC_RAISESIGNAL, "DC_RAISESIGNAL"},
#define DC_CONFIG_PERSIST	(DC_BASE+2)
		{DC_CONFIG_PERSIST, "DC_CONFIG_PERSIST"},
#define DC_CONFIG_RUNTIME	(DC_BASE+3)
		{DC_CONFIG_RUNTIME, "DC_CONFIG_RUNTIME"},
#define DC_RECONFIG			(DC_BASE+4)
		{DC_RECONFIG, "DC_RECONFIG"},
#define DC_OFF_GRACEFUL		(DC_BASE+5)
		{DC_OFF_GRACEFUL, "DC_OFF_GRACEFUL"},
#define DC_OFF_FAST			(DC_BASE+6)
		{DC_OFF_FAST, "DC_OFF_FAST"},
#define DC_CONFIG_VAL		(DC_BASE+7)
		{DC_CONFIG_VAL, "DC_CONFIG_VAL"},
#define DC_CHILDALIVE		(DC_BASE+8)
		{DC_CHILDALIVE, "DC_CHILDALIVE"},
#define DC_SERVICEWAITPIDS	(DC_BASE+9)
		{DC_SERVICEWAITPIDS, "DC_SERVICEWAITPIDS"},
#define DC_AUTHENTICATE     (DC_BASE+10)
		{DC_AUTHENTICATE, "DC_AUTHENTICATE"},
#define DC_NOP              (DC_BASE+11)
		{DC_NOP, "DC_NOP"},
#define DC_RECONFIG_FULL	(DC_BASE+12)
		{DC_RECONFIG_FULL, "DC_RECONFIG_FULL"},
#define DC_FETCH_LOG        (DC_BASE+13)
		{DC_FETCH_LOG, "DC_FETCH_LOG"},
#define DC_INVALIDATE_KEY   (DC_BASE+14)
		{DC_INVALIDATE_KEY, "DC_INVALIDATE_KEY"},
#define DC_OFF_PEACEFUL     (DC_BASE+15)
		{DC_OFF_PEACEFUL, "DC_OFF_PEACEFUL"},
#define DC_SET_PEACEFUL_SHUTDOWN (DC_BASE+16)
		{DC_SET_PEACEFUL_SHUTDOWN, "DC_SET_PEACEFUL_SHUTDOWN"},
#define DC_TIME_OFFSET      (DC_BASE+17)
		{DC_TIME_OFFSET, "DC_TIME_OFFSET"},
#define DC_PURGE_LOG        (DC_BASE+18)
		{DC_PURGE_LOG, "DC_PURGE_LOG"},
		//#define DC_SHARE_SOCK       (DC_BASE+19)		/* Not used */

		// these are all NOP but registered at different authz levels
#define DC_NOP_READ         (DC_BASE+20)
		{DC_NOP_READ, "DC_NOP_READ"},
#define DC_NOP_WRITE        (DC_BASE+21)
		{DC_NOP_WRITE, "DC_NOP_WRITE"},
#define DC_NOP_NEGOTIATOR   (DC_BASE+22)
		{DC_NOP_NEGOTIATOR, "DC_NOP_NEGOTIATOR"},
#define DC_NOP_ADMINISTRATOR (DC_BASE+23)
		{DC_NOP_ADMINISTRATOR, "DC_NOP_ADMINISTRATOR"},
#define DC_NOP_OWNER        (DC_BASE+24)
		{DC_NOP_OWNER, "DC_NOP_OWNER"},
#define DC_NOP_CONFIG       (DC_BASE+25)
		{DC_NOP_CONFIG, "DC_NOP_CONFIG"},
#define DC_NOP_DAEMON       (DC_BASE+26)
		{DC_NOP_DAEMON, "DC_NOP_DAEMON"},
#define DC_NOP_ADVERTISE_STARTD (DC_BASE+27)
		{DC_NOP_ADVERTISE_STARTD, "DC_NOP_ADVERTISE_STARTD"},
#define DC_NOP_ADVERTISE_SCHEDD (DC_BASE+28)
		{DC_NOP_ADVERTISE_SCHEDD, "DC_NOP_ADVERTISE_SCHEDD"},
#define DC_NOP_ADVERTISE_MASTER (DC_BASE+29)
		{DC_NOP_ADVERTISE_MASTER, "DC_NOP_ADVERTISE_MASTER"},
		// leave 30-39 open -- zmiller
#define DC_SEC_QUERY        (DC_BASE+40)
		{DC_SEC_QUERY, "DC_SEC_QUERY"},
#define DC_SET_FORCE_SHUTDOWN (DC_BASE+41)
		{DC_SET_FORCE_SHUTDOWN, "DC_SET_FORCE_SHUTDOWN"},
#define DC_OFF_FORCE       (DC_BASE+42)
		{DC_OFF_FORCE, "DC_OFF_FORCE"},
#define DC_SET_READY       (DC_BASE+43)  // sent to parent to indicate a demon is ready for use
		{DC_SET_READY, "DC_SET_READY"},
#define DC_QUERY_READY     (DC_BASE+44)  // daemon command handler should reply only once it and children are ready
		{DC_QUERY_READY, "DC_QUERY_READY"},
#define DC_QUERY_INSTANCE  (DC_BASE+45)  // ask if a daemon is alive - returns a random 64 bit int that will not change as long as this instance is alive.
		{DC_QUERY_INSTANCE, "DC_QUERY_INSTANCE"},
#define DC_GET_SESSION_TOKEN (DC_BASE+46) // Retrieve an authentication token for TOKEN that is at most equivalent to the current session.
		{DC_GET_SESSION_TOKEN, "DC_GET_SESSION_TOKEN"},
#define DC_START_TOKEN_REQUEST (DC_BASE+47) // Request a token from this daemon.
		{DC_START_TOKEN_REQUEST, "DC_START_TOKEN_REQUEST"},
#define DC_FINISH_TOKEN_REQUEST (DC_BASE+48) // Poll remote daemon for available token.
		{DC_FINISH_TOKEN_REQUEST, "DC_FINISH_TOKEN_REQUEST"},
#define DC_LIST_TOKEN_REQUEST (DC_BASE+49) // Poll for the existing token requests.
		{DC_LIST_TOKEN_REQUEST, "DC_LIST_TOKEN_REQUEST"},
#define DC_APPROVE_TOKEN_REQUEST (DC_BASE+50) // Approve a token request.
		{DC_APPROVE_TOKEN_REQUEST, "DC_APPROVE_TOKEN_REQUEST"},
#define DC_AUTO_APPROVE_TOKEN_REQUEST (DC_BASE+51) // Auto-approve token requests.
		{DC_AUTO_APPROVE_TOKEN_REQUEST, "DC_AUTO_APPROVE_TOKEN_REQUEST"},
#define DC_EXCHANGE_SCITOKEN (DC_BASE+52) // Exchange a SciToken for a Condor token.
		{DC_EXCHANGE_SCITOKEN, "DC_EXCHANGE_SCITOKEN"},

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
		{FILETRANS_UPLOAD,"FILETRANS_UPLOAD"},
#define FILETRANS_DOWNLOAD (FILETRANSFER_BASE+1)
		{FILETRANS_DOWNLOAD,"FILETRANS_DOWNLOAD"},


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
//#define SHADOW_UPDATEINFO	   (DCSHADOW_BASE+0)
//		{SHADOW_UPDATEINFO, "SHADOW_UPDATEINFO"},
//#define TAKE_MATCH             (DCSHADOW_BASE+1)  // for MPI & parallel shadow, Not used
#define MPI_START_COMRADE      (DCSHADOW_BASE+2)  // for MPI & parallel shadow
		{MPI_START_COMRADE, "MPI_START_COMRADE"},
#define GIVE_MATCHES 	       (DCSHADOW_BASE+3)  // for MPI & parallel shadow
		{GIVE_MATCHES, "GIVE_MATCHES"},
//#define RECEIVE_JOBAD		   (DCSHADOW_BASE+4)	/* Not used */
#define UPDATE_JOBAD		   (DCSHADOW_BASE+5)
		{UPDATE_JOBAD, "UPDATE_JOBAD"},


/*
*** Used only in THE TOOL to choose the condor_squawk option.
*/
#define SQUAWK 72000
		{SQUAWK, "SQUAWK"},

/*
*** Commands used by the gridmanager daemon
*/
#define DCGRIDMANAGER_BASE 73000
#define GRIDMAN_CHECK_LEASES (DCGRIDMANAGER_BASE+0)
		{GRIDMAN_CHECK_LEASES, "GRIDMAN_CHECK_LEASES"},
#define GRIDMAN_REMOVE_JOBS SIGUSR1
#define GRIDMAN_ADD_JOBS SIGUSR2

/*
*** Commands used by the transfer daemon
*/
//#define TRANSFERD_BASE 74000
/* This is used by the schedd when a transferd registers itself */
//#define TRANSFERD_REGISTER		(TRANSFERD_BASE+0)	// Not used
/* a channel under which a transferd may be sent control message such as
	being informed of a new a new transfer request 
*/
//#define TRANSFERD_CONTROL_CHANNEL	(TRANSFERD_BASE+1)	// Not used
/* Files are being written to the transferd for storage */
//#define TRANSFERD_WRITE_FILES	(TRANSFERD_BASE+2)	// Not used
/* files are being read from the transferd's storage */
//#define TRANSFERD_READ_FILES	(TRANSFERD_BASE+3)	// Not used


/*
*** Commands used by the credd daemon
*/
#define CREDD_BASE 81000
#define CREDD_STORE_CRED (CREDD_BASE+0)
		{CREDD_STORE_CRED, "CREDD_STORE_CRED"},
#define CREDD_GET_CRED (CREDD_BASE+1)
		{CREDD_GET_CRED, "CREDD_GET_CRED"},
#define CREDD_REMOVE_CRED (CREDD_BASE+2)
		{CREDD_REMOVE_CRED, "CREDD_REMOVE_CRED"},
#define CREDD_QUERY_CRED (CREDD_BASE+3)
		{CREDD_QUERY_CRED, "CREDD_QUERY_CRED"},
#define CREDD_REFRESH_ALL (CREDD_BASE+20)	// Not used
		{CREDD_REFRESH_ALL, "CREDD_REFRESH_ALL"},
#define CREDD_CHECK_CREDS (CREDD_BASE+30)	// check to see if the desired oauth tokens are stored
		{CREDD_CHECK_CREDS, "CREDD_CHECK_CREDS"},
#define CREDD_GET_PASSWD (CREDD_BASE+99)	// used by the Win32 credd only
		{CREDD_GET_PASSWD, "CREDD_GET_PASSWD"},
#define CREDD_NOP (CREDD_BASE+100)			// used by the Win32 credd only
		{CREDD_NOP, "CREDD_NOP"},
#define COMMAND_LAST (INT_MAX)			// used by the Win32 credd only
		{COMMAND_LAST, "COMMAND_LAST"},
	}};
}

// validate that the size of the array is correct...
static_assert(makeCommandTable().back().first == COMMAND_LAST, "Is the size of the std::array correct?");

		// values used for "HowFast" in the draining request
#define DRAIN_GRACEFUL 0  // retirement time and vacate time are honored
#define DRAIN_QUICK 10	  // retirement time will not be honored, but vacate time will
#define DRAIN_FAST 20	  // neither retirement time nor vacate time will be honored

		// values for OnCompletion for draining request
		// note that prior to 8.9.11 only Resume and Nothing are recognised
#define DRAIN_NOTHING_ON_COMPLETION 0
#define DRAIN_RESUME_ON_COMPLETION  1
#define DRAIN_EXIT_ON_COMPLETION    2
#define DRAIN_RESTART_ON_COMPLETION 3
#define DRAIN_RECONFIG_ON_COMPLETION 4 // New for 9.11.0

		// HAD-related commands

/************
*** Command ids used by the collector 
************/
constexpr const
std::array<std::pair<int, const char *>, 63> makeCollectorCommandTable() {
	return {{ 
#define UPDATE_STARTD_AD		0
		{UPDATE_STARTD_AD, "UPDATE_STARTD_AD"},
#define UPDATE_SCHEDD_AD		1
		{UPDATE_SCHEDD_AD, "UPDATE_SCHEDD_AD"},
#define UPDATE_MASTER_AD		2
		{UPDATE_MASTER_AD, "UPDATE_MASTER_AD"},
//#define UPDATE_GATEWAY_AD		3;			/* Not used */
//		{UPDATE_GATEWAY_AD, "UPDATE_GATEWAY_AD"},
#define UPDATE_CKPT_SRVR_AD	4
		{UPDATE_CKPT_SRVR_AD, "UPDATE_CKPT_SRVR_AD"},

#define QUERY_STARTD_ADS		5
		{QUERY_STARTD_ADS, "QUERY_STARTD_ADS"},
#define QUERY_SCHEDD_ADS		6
		{QUERY_SCHEDD_ADS, "QUERY_SCHEDD_ADS"},
#define QUERY_MASTER_ADS		7
		{QUERY_MASTER_ADS, "QUERY_MASTER_ADS"},
//#define QUERY_GATEWAY_ADS		8;			/* Not used */
//		{QUERY_GATEWAY_ADS, "QUERY_GATEWAY_ADS"},
#define QUERY_CKPT_SRVR_ADS	9
		{QUERY_CKPT_SRVR_ADS, "QUERY_CKPT_SRVR_ADS"},
#define QUERY_STARTD_PVT_ADS	10
		{QUERY_STARTD_PVT_ADS, "QUERY_STARTD_PVT_ADS"},

#define UPDATE_SUBMITTOR_AD	11
		{UPDATE_SUBMITTOR_AD, "UPDATE_SUBMITTOR_AD"},
#define QUERY_SUBMITTOR_ADS	12
		{QUERY_SUBMITTOR_ADS, "QUERY_SUBMITTOR_ADS"},

#define INVALIDATE_STARTD_ADS	13
		{INVALIDATE_STARTD_ADS, "INVALIDATE_STARTD_ADS"},
#define INVALIDATE_SCHEDD_ADS	14
		{INVALIDATE_SCHEDD_ADS, "INVALIDATE_SCHEDD_ADS"},
#define INVALIDATE_MASTER_ADS	15
		{INVALIDATE_MASTER_ADS, "INVALIDATE_MASTER_ADS"},
//#define INVALIDATE_GATEWAY_ADS	16;			/* Not used */
//		{INVALIDATE_GATEWAY_ADS, "INVALIDATE_GATEWAY_ADS"},
#define INVALIDATE_CKPT_SRVR_ADS	17
		{INVALIDATE_CKPT_SRVR_ADS, "INVALIDATE_CKPT_SRVR_ADS"},
#define INVALIDATE_SUBMITTOR_ADS	18
		{INVALIDATE_SUBMITTOR_ADS, "INVALIDATE_SUBMITTOR_ADS"},

#define UPDATE_COLLECTOR_AD	19
		{UPDATE_COLLECTOR_AD, "UPDATE_COLLECTOR_AD"},
#define QUERY_COLLECTOR_ADS	20
		{QUERY_COLLECTOR_ADS, "QUERY_COLLECTOR_ADS"},
#define INVALIDATE_COLLECTOR_ADS	21
		{INVALIDATE_COLLECTOR_ADS, "INVALIDATE_COLLECTOR_ADS"},

#define QUERY_HIST_STARTD 22
		{QUERY_HIST_STARTD, "QUERY_HIST_STARTD"},
#define QUERY_HIST_STARTD_LIST 23
		{QUERY_HIST_STARTD_LIST, "QUERY_HIST_STARTD_LIST"},
#define QUERY_HIST_SUBMITTOR 24
		{QUERY_HIST_SUBMITTOR, "QUERY_HIST_SUBMITTOR"},
#define QUERY_HIST_SUBMITTOR_LIST 25
		{QUERY_HIST_SUBMITTOR_LIST, "QUERY_HIST_SUBMITTOR_LIST"},
#define QUERY_HIST_GROUPS 26
		{QUERY_HIST_GROUPS, "QUERY_HIST_GROUPS"},
#define QUERY_HIST_GROUPS_LIST 27
		{QUERY_HIST_GROUPS_LIST, "QUERY_HIST_GROUPS_LIST"},
#define QUERY_HIST_SUBMITTORGROUPS 28
		{QUERY_HIST_SUBMITTORGROUPS, "QUERY_HIST_SUBMITTORGROUPS"},
#define QUERY_HIST_SUBMITTORGROUPS_LIST 29
		{QUERY_HIST_SUBMITTORGROUPS_LIST, "QUERY_HIST_SUBMITTORGROUPS_LIST"},
#define QUERY_HIST_CKPTSRVR 30
		{QUERY_HIST_CKPTSRVR, "QUERY_HIST_CKPTSRVR"},
#define QUERY_HIST_CKPTSRVR_LIST 31
		{QUERY_HIST_CKPTSRVR_LIST, "QUERY_HIST_CKPTSRVR_LIST"},

#define UPDATE_LICENSE_AD			42
		{UPDATE_LICENSE_AD, "UPDATE_LICENSE_AD"},
#define QUERY_LICENSE_ADS			43
		{QUERY_LICENSE_ADS, "QUERY_LICENSE_ADS"},
#define INVALIDATE_LICENSE_ADS	44
		{INVALIDATE_LICENSE_ADS, "INVALIDATE_LICENSE_ADS"},

#define UPDATE_STORAGE_AD 45
		{UPDATE_STORAGE_AD, "UPDATE_STORAGE_AD"},
#define QUERY_STORAGE_ADS 46
		{QUERY_STORAGE_ADS, "QUERY_STORAGE_ADS"},
#define INVALIDATE_STORAGE_ADS 47
		{INVALIDATE_STORAGE_ADS, "INVALIDATE_STORAGE_ADS"},

#define QUERY_ANY_ADS 48
		{QUERY_ANY_ADS, "QUERY_ANY_ADS"},

#define UPDATE_NEGOTIATOR_AD 	49
		{UPDATE_NEGOTIATOR_AD, "UPDATE_NEGOTIATOR_AD"},
#define QUERY_NEGOTIATOR_ADS	50
		{QUERY_NEGOTIATOR_ADS, "QUERY_NEGOTIATOR_ADS"},
#define INVALIDATE_NEGOTIATOR_ADS 51
		{INVALIDATE_NEGOTIATOR_ADS, "INVALIDATE_NEGOTIATOR_ADS"},

#define QUERY_MULTIPLE_ADS		53
		{QUERY_MULTIPLE_ADS, "QUERY_MULTIPLE_ADS"},
#define QUERY_MULTIPLE_PVT_ADS	54
		{QUERY_MULTIPLE_PVT_ADS, "QUERY_MULTIPLE_PVT_ADS"},

#define UPDATE_HAD_AD 55
		{UPDATE_HAD_AD, "UPDATE_HAD_AD"},
#define QUERY_HAD_ADS 56
		{QUERY_HAD_ADS, "QUERY_HAD_ADS"},
#define INVALIDATE_HAD_ADS 57
		{INVALIDATE_HAD_ADS, "INVALIDATE_HAD_ADS"},

#define UPDATE_AD_GENERIC 58
		{UPDATE_AD_GENERIC, "UPDATE_AD_GENERIC"},
#define INVALIDATE_ADS_GENERIC 59
		{INVALIDATE_ADS_GENERIC, "INVALIDATE_ADS_GENERIC"},

#define UPDATE_STARTD_AD_WITH_ACK 60
		{UPDATE_STARTD_AD_WITH_ACK, "UPDATE_STARTD_AD_WITH_ACK"},

//#define UPDATE_XFER_SERVICE_AD		61;	/* Not used */
//		{UPDATE_XFER_SERVICE_AD, "UPDATE_XFER_SERVICE_AD"},
//#define QUERY_XFER_SERVICE_ADS		62;	/* Not used */
//		{QUERY_XFER_SERVICE_ADS, "QUERY_XFER_SERVICE_ADS"},
//#define INVALIDATE_XFER_SERVICE_ADS	63;	/* Not used */
//			{INVALIDATE_XFER_SERVICE_ADS, "INVALIDATE_XFER_SERVICE_ADS"},

//#define UPDATE_LEASE_MANAGER_AD		64;	/* Not used */
//			{UPDATE_LEASE_MANAGER_AD, "UPDATE_LEASE_MANAGER_AD"},
//#define QUERY_LEASE_MANAGER_ADS		65;	/* Not used */
//			{QUERY_LEASE_MANAGER_ADS, "QUERY_LEASE_MANAGER_ADS"},
//#define INVALIDATE_LEASE_MANAGER_ADS  66;	/* Not used */
//			{INVALIDATE_LEASE_MANAGER_ADS, "INVALIDATE_LEASE_MANAGER_ADS"},

#define CCB_REGISTER 67
		{CCB_REGISTER, "CCB_REGISTER"},
#define CCB_REQUEST 68
		{CCB_REQUEST, "CCB_REQUEST"},
#define CCB_REVERSE_CONNECT 69
		{CCB_REVERSE_CONNECT, "CCB_REVERSE_CONNECT"},

#define UPDATE_GRID_AD 	70
		{UPDATE_GRID_AD, "UPDATE_GRID_AD"},
#define QUERY_GRID_ADS	71
		{QUERY_GRID_ADS, "QUERY_GRID_ADS"},
#define INVALIDATE_GRID_ADS 72
		{INVALIDATE_GRID_ADS, "INVALIDATE_GRID_ADS"},

#define MERGE_STARTD_AD 73
		{MERGE_STARTD_AD, "MERGE_STARTD_AD"},
#define QUERY_GENERIC_ADS 74
		{QUERY_GENERIC_ADS, "QUERY_GENERIC_ADS"},

#define SHARED_PORT_CONNECT 75
		{SHARED_PORT_CONNECT, "SHARED_PORT_CONNECT"},
#define SHARED_PORT_PASS_SOCK 76
		{SHARED_PORT_PASS_SOCK, "SHARED_PORT_PASS_SOCK"},

#define UPDATE_ACCOUNTING_AD 77
		{UPDATE_ACCOUNTING_AD, "UPDATE_ACCOUNTING_AD"},
#define QUERY_ACCOUNTING_ADS 78
		{QUERY_ACCOUNTING_ADS, "QUERY_ACCOUNTING_ADS"},
#define INVALIDATE_ACCOUNTING_ADS 79
		{INVALIDATE_ACCOUNTING_ADS, "INVALIDATE_ACCOUNTING_ADS"},

#define UPDATE_OWN_SUBMITTOR_AD 80
		{UPDATE_OWN_SUBMITTOR_AD, "UPDATE_OWN_SUBMITTOR_AD"},

			// Request a collector to retrieve an identity token from a schedd.
#define IMPERSONATION_TOKEN_REQUEST 81
		{IMPERSONATION_TOKEN_REQUEST, "IMPERSONATION_TOKEN_REQUEST"},

#define COLLECTOR_COMMAND_LAST (INT_MAX - 1)			// used by the Win32 credd only
		{COLLECTOR_COMMAND_LAST, "COLLECTOR_COMMAND_LAST"},
	}};
}

static_assert(makeCollectorCommandTable().back().first == COLLECTOR_COMMAND_LAST, "Is the size of the std::array correct?");
#endif  /* if ifndef _CONDOR_COMMANDS_H */
