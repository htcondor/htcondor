#ifndef _MACH_STATUS
#define _MACH_STATUS


#if !defined(__STDC__) && !defined(__cplusplus)
#define const
#endif

static const int	NO_JOB			= 0;
static const int	JOB_RUNNING		= 1;
static const int	KILLED			= 2;
static const int	CHECKPOINTING	= 3;
static const int	SUSPENDED		= 4;
static const int	USER_BUSY		= 5;
static const int	M_IDLE			= 6;
static const int	BLOCKED			= 7;
static const int	SYSTEM			= 8;
static const int	CONDOR_DOWN		= - 1;

#if !defined(__STDC__) && !defined(__cplusplus)
#undef const
#endif



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
