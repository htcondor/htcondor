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
	int			flock_version;
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
	PROC_ID		id;
	char		*pool_name;
	int		time_stamp;
} SERVING;

/*
** Datastructure used by W-Schedd to store requests.
*/

typedef struct request {
	struct request	*next;
	struct request	*prev;
	char		*pool_name;
	char		*machine_name;
	PROC_ID		id;
	CONTEXT		*job_context;
	int		result;
	char		*server;
} REQUEST;
#endif /* CONDOR_WORLD */
