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
#define JOB_EXITED		0	/* The job exited (not killed)                   */
#define JOB_CKPTED		1	/* The job was checkpointed                      */
#define JOB_KILLED		2	/* The job was killed                            */
#define JOB_COREDUMPED	3	/* The job was killed and a core file produced   */
#define JOB_EXCEPTION	4	/* The job exited with an exception              */
#define JOB_NO_MEM		5	/* Not enough memory to start the shadow 		 */
#define JOB_SHADOW_USAGE 6	/* incorrect arguments to condor_shadow		 */
#define JOB_NOT_CKPTED	7	/* The job was kicked off without a checkpoint	 */
#define JOB_NOT_STARTED	8	/* Can't connect to startd or request refused	 */
#define JOB_BAD_STATUS	9	/* Job status != RUNNING on startup				 */
#define JOB_EXEC_FAILED 10  /* Exec failed for some reason other than ENOMEM */
#define JOB_NO_CKPT_FILE 11 /* There is no checkpoint file (lost)            */
#define JOB_SHOULD_HOLD 12	/* The job should be put on hold                 */

#define DPRINTF_ERROR 44	/* There is a fatal error with dprintf()         */
