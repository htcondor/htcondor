/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
/*****************************************************************************
*                                                                            *
*                         World Machine header file                          *
*                              >>> world.h <<<                               *
*                                                                            *
*                          Computer Systeem Groep                            *
*         Nationaal Instituut voor Kernfysica en Hoge-Energiefysica          *
*             Kruislaan 409, 1098 SJ Amsterdam, The Netherlands              *
*                                                                            *
*                                VERSION 1.1b                                *
*                          Written by: Xander Evers                          *
*                       Last changed: 24-05-1994 15:00                       *
*                                                                            *
*****************************************************************************/
#ifndef CONDOR_WORLD
#define CONDOR_WORLD
/*
** Should be moved to the sched.h file.
*/

#define FREE_MACHINES       (SCHED_VERS+35)
#define FOUND_MACHINE       (SCHED_VERS+36)
#define NOT_FOUND       (SCHED_VERS+37)
#define REQUEST_MACHINE     (SCHED_VERS+38)
#define MACHINE         (SCHED_VERS+39)
#define NO_MACHINE      (SCHED_VERS+40)
#define GIVE_MACHINE        (SCHED_VERS+41)


/*
** Datastructure used by W-Startd and W-Schedd to store pool configuration.
*/

#define	MAX_POOLS	25

typedef struct config_rec {
	char		*pool_name;
	char		in;
	char		out;
	struct in_addr	net_addr;
	short		net_addr_type;
} CONFIG_REC;

/*
** Datastructure used by the W-Startd to store the list of free machines.
*/

typedef struct free_mach {
	struct free_mach *next;
	struct free_mach *prev;
	char		*name;
	CONTEXT		*machine_context;
} FREE_MACH;

typedef struct free_mach_list {
	struct free_mach_list *next;
	struct free_mach_list *prev;
	char		*pool_name;
	int		time_stamp;
	FREE_MACH	*free_machines;
} FREE_MACH_LIST;

/*
** Datastructure used by W-Startd to store requests.
*/

typedef struct serving {
	struct serving	*next;
	struct serving	*prev;
	char		*machine_name;
	int		    id;                 /* dhruba */
	char		*pool_name;
	int		time_stamp;
} SERVING;

/*
** Datastructure used by W-Schedd to store requests.
*/

typedef struct request {
	struct request	*next;
	struct request	*prev;
	int			cmd;
	char		*pool_name;
	char		*machine_name;
	int			id;            /* world condor id : dhruba */
	CONTEXT		*job_context;
	int			result;
	StartdRec	stRec;
	int			child_fd;     /* pipe from world-schedd to child */
} REQUEST;

/*
**	Info transferred from the W-Schedd to the W-Shadow
*/
#define 	MAXHOSTLEN		1024 /* maximum length of a machine name */
typedef struct shadow_interface
{
	int		result;              /* result of negotiations    */
	char	server[MAXHOSTLEN];  /* name of executing machine */
} ShadowRec;

#define STARTER_0 (SCHED_VERS+ALT_STARTER_BASE+0)
#define STARTER_1 (SCHED_VERS+ALT_STARTER_BASE+1)
#define STARTER_2 (SCHED_VERS+ALT_STARTER_BASE+2)
#define STARTER_3 (SCHED_VERS+ALT_STARTER_BASE+3)
#define STARTER_4 (SCHED_VERS+ALT_STARTER_BASE+4)
#define STARTER_5 (SCHED_VERS+ALT_STARTER_BASE+5)
#define STARTER_6 (SCHED_VERS+ALT_STARTER_BASE+6)
#define STARTER_7 (SCHED_VERS+ALT_STARTER_BASE+7)
#define STARTER_8 (SCHED_VERS+ALT_STARTER_BASE+8)
#define STARTER_9 (SCHED_VERS+ALT_STARTER_BASE+9)
#endif /* CONDOR_WORLD */
