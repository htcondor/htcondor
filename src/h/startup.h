typedef struct {
	int		cluster;				/* Condor Cluster # */
	int		proc;					/* Condor Proc # */
	int		job_class;				/* STANDARD, VANILLA, PVM, PIPE, ... */
	uid_t	uid;					/* Execute job under this UID */
	gid_t	gid;					/* Execute job under this pid */
	pid_t	virt_pid;				/* PVM virt pid of this process */
	int		soft_kill_sig;			/* Use this signal for a soft kill */
	char	*a_out_name;			/* Where to get executable file */
	char	*cmd;					/* Command name given by the user */
	char	*args;					/* Command arguments given by the user */
	char	*env;					/* Environment variables */
	char	*iwd;					/* Initial working directory */
	BOOLEAN	ckpt_wanted;			/* Whether user wants checkpointing */
	BOOLEAN	is_restart;				/* Whether this run is a restart */
	BOOLEAN	coredump_limit_exists;	/* Whether user wants to limit core size */
	int		coredump_limit;			/* Limit in bytes */
} STARTUP_INFO;

/*
Should eliminate starter knowing the a.out name.  Should go to
shadow for instructions on how to fetch the executable - e.g. local file
with name <name> or get it from TCP port <x> on machine <y>.
*/
