#ifndef _MACH_STATUS
#define _MACH_STATUS


#define NO_JOB			0
#define JOB_RUNNING		1
#define KILLED			2
#define CHECKPOINTING	3
#define SUSPENDED		4
#define USER_BUSY		5
#define M_IDLE			6
#define BLOCKED			7
#define SYSTEM			8
#define CONDOR_DOWN		-1


#if defined( __STDC__) || defined(__cplusplus)	/* ANSI Prototypes */

#if defined(__cplusplus)
extern "C" {
#endif

int get_machine_status ( void );

#if defined(__cplusplus)
}
#endif

#else	/* Non ANSI Prototypes */

int get_machine_status();

#endif


#endif /* _MACH_STATUS */
