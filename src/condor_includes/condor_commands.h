/* Copyright 1997, Condor Team */

#ifndef _CONDOR_COMMANDS_H
#define _CONDOR_COMMANDS_H

/*
*** Daemon Core Commands
*/
#define DC_BASE 60000
#define DC_RAISESIGNAL (DC_BASE+0)

/****
** Queue Manager Commands
****/
#define QMGMT_CMD	1111

/* Scheduler Commands */
/*
**	Scheduler version number
*/
#define SCHED_VERS			400
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

#endif  /* of ifndef _CONDOR_COMMANDS_H */
