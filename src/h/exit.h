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

 


/*
**	Shadow exit statuses which reflect the exit status of the job
*/

/* ok, because of strange exit codes from the util lib and such(which
	shouldn't be there, but that story is for another time), we can't
	be totally sure that the exit values with low numbers will mean
	what we think they mean. So I'm offsetting the exit codes from the
	shadow so that we know for sure(relatively anyway) what happened.
	Any exit code less than the offset should be treated as a shadow
	exception. Please make sure you don't roll over 255 in the return
	code. For legacy reasons DPRINTF_ERROR and JOB_EXCEPTION must
	remain the same. */

#define EXIT_CODE_OFFSET 100

/* The job exited (not killed) */
#define JOB_EXITED		(0 + EXIT_CODE_OFFSET)

/* The job was checkpointed */
#define JOB_CKPTED		(1 + EXIT_CODE_OFFSET)

/* The job was killed */
#define JOB_KILLED		(2 + EXIT_CODE_OFFSET)

/* The job was killed and a core file produced */
#define JOB_COREDUMPED	(3 + EXIT_CODE_OFFSET)

/* The job exited with an exception */
#define JOB_EXCEPTION	4

/* Not enough memory to start the shadow */
#define JOB_NO_MEM		(5 + EXIT_CODE_OFFSET)

/* incorrect arguments to condor_shadow */
#define JOB_SHADOW_USAGE (6 + EXIT_CODE_OFFSET)

/* The job was kicked off without a checkpoint */
#define JOB_NOT_CKPTED	(7 + EXIT_CODE_OFFSET)

/* We define this to the same number, since we want the same behavior.
   However, "JOB_NOT_CKPTED" doesn't mean much if we're not a standard
   universe job.  The effect of this exit code is that we want the job
   to be put back in the job queue and run again. 
*/
#define JOB_SHOULD_REQUEUE	(7 + EXIT_CODE_OFFSET)

/* Can't connect to startd or request refused */
#define JOB_NOT_STARTED	(8 + EXIT_CODE_OFFSET)

/* Job status != RUNNING on startup */
#define JOB_BAD_STATUS	(9 + EXIT_CODE_OFFSET)

/* Exec failed for some reason other than ENOMEM */
#define JOB_EXEC_FAILED (10 + EXIT_CODE_OFFSET)

/* There is no checkpoint file (lost) */
#define JOB_NO_CKPT_FILE (11 + EXIT_CODE_OFFSET)

/* The job should be put on hold */
#define JOB_SHOULD_HOLD (12 + EXIT_CODE_OFFSET)

/* The job should be removed */
#define JOB_SHOULD_REMOVE (13 + EXIT_CODE_OFFSET)

/* There is a fatal error with dprintf() */
#define DPRINTF_ERROR 44


