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

/*
*** Daemon Core Signals
*/
// Generic Unix signals.
// defines for signals; compatibility with traditional UNIX 
// values maintained where possible.
#define	DC_SIGHUP	1	/* hangup */
#define	DC_SIGINT	2	/* interrupt (rubout) */
#define	DC_SIGQUIT	3	/* quit (ASCII FS) */
#define	DC_SIGILL	4	/* illegal instruction (not reset when caught) */
#define	DC_SIGTRAP	5	/* trace trap (not reset when caught) */
#define	DC_SIGIOT	6	/* IOT instruction */
#define	DC_SIGABRT	6	/* used by abort, replace DC_SIGIOT in the future */
#define	DC_SIGEMT	7	/* EMT instruction */
#define	DC_SIGFPE	8	/* floating point exception */
#define	DC_SIGKILL	9	/* kill (cannot be caught or ignored) */
#define	DC_SIGBUS	10	/* bus error */
#define	DC_SIGSEGV	11	/* segmentation violation */
#define	DC_SIGSYS	12	/* bad argument to system call */
#define	DC_SIGPIPE	13	/* write on a pipe with no one to read it */
#define	DC_SIGALRM	14	/* alarm clock */
#define	DC_SIGTERM	15	/* software termination signal from kill */
#define	DC_SIGUSR1	16	/* user defined signal 1 */
#define	DC_SIGUSR2	17	/* user defined signal 2 */
#define	DC_SIGCLD	18	/* child status change */
#define	DC_SIGCHLD	18	/* child status change alias (POSIX) */
#define	DC_SIGPWR	19	/* power-fail restart */
#define	DC_SIGWINCH 20	/* window size change */
#define	DC_SIGURG	21	/* urgent socket condition */
#define	DC_SIGPOLL 22	/* pollable event occured */
#define	DC_SIGIO	DC_SIGPOLL	/* socket I/O possible (DC_SIGPOLL alias) */
#define	DC_SIGSTOP 23	/* stop (cannot be caught or ignored) */
#define	DC_SIGTSTP 24	/* user stop requested from tty */
#define	DC_SIGCONT 25	/* stopped process has been continued */
#define DC_SIGTTIN 26
#define DC_SIGTTOU 27
#define DC_SIGXCPU 28
#define DC_SIGXFSZ 29
#define DC_SIGVTALRM 30
#define DC_SIGPROF 31
#define DC_SIGINFO 32

// Signals used for Startd -> Starter communication
#define DC_SIGSUSPEND	100
#define DC_SIGCONTINUE	101
#define DC_SIGSOFTKILL	102	// vacate w/ checkpoint
#define DC_SIGHARDKILL	103 // kill w/o checkpoint
#define DC_SIGPCKPT		104	// periodic checkpoint

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
#define SHADOW_UPDATEINFO	(DCSHADOW_BASE+0)
#define TAKE_MATCH          (DCSHADOW_BASE+1)  // for MPI shadow
#define MPI_START_COMRADE   (DCSHADOW_BASE+2)  // for MPI shadow
#define GIVE_MATCHES 	    (DCSHADOW_BASE+3)  // for MPI shadow


/*
*** Used only in THE TOOL to choose the condor_squawk option.
*/
#define SQUAWK 72000

/*
*** Commands used by the gridmanager daemon
*/
#define DCGRIDMANAGER_BASE 73000
#define GRIDMAN_REMOVE_JOBS DC_SIGUSR1
#define GRIDMAN_ADD_JOBS DC_SIGUSR2

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


#endif  /* of ifndef _CONDOR_COMMANDS_H */
