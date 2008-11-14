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


 
#ifndef CONDOR_EXIT_UTILS_H
#define CONODR_EXIT_UTILS_H


/*
**	Shadow exit statuses which reflect the exit status of the job
**  Also, routines to create an exit status
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

/* used for general exception exiting testing for daemons */
#define EXIT_EXCEPTION	JOB_EXCEPTION

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

/* The job missed its deferred execution time */
#define JOB_MISSED_DEFERRAL_TIME (14 + EXIT_CODE_OFFSET)

/* Same as JOB_EXITED, but also inform schedd that the claim will
   not accept any more jobs. */
#define JOB_EXITED_AND_CLAIM_CLOSING (15 + EXIT_CODE_OFFSET)

/*
  WARNING: don't go above 27 with these, or we run out of bits in the
  exit status code
*/

/* There is a fatal error with dprintf() */
#define DPRINTF_ERROR 44


/***********************************************************************
  Exit codes for DaemonCore daemons to communicate with the
  condor_master.  To avoid potential conflicts with the above, and
  since we only anticipate a tiny number of these, we count down from
  the EXIT_CODE_OFFSET (100).
***********************************************************************/

/* The daemon exited and does not want to be restarted automatically. */
#define DAEMON_NO_RESTART (EXIT_CODE_OFFSET - 1)


BEGIN_C_DECLS
int generate_exit_code( int input_code );

int generate_exit_signal( int input_signal );

END_C_DECLS

#endif /*CONDOR_EXIT_UTILS*/
