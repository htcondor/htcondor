#ifndef _CONDOR_CONDOR_RESOURCE_H
#define _CONDOR_CONDOR_RESOURCE_H

typedef char *resource_name_t;
typedef char *resource_id_t;
typedef char *job_id_t;
typedef char *task_id_t;

#define NO_RNAME	(resource_name_t)0
#define NO_RID		(resource_id_t)0
#define NO_JID		(job_id_t)0
#define NO_TID		(task_id_t)0
#define NO_PID		(-1)

struct resparam {
	double		res_load;
	unsigned long	res_memavail;
	unsigned long	res_diskavail;
	int		res_idle;
};

struct jobparam {
	int	job_prio;
	int	job_state;
	int	job_memuse;
	int	job_diskuse;
	int	job_ntasks;
};

struct taskparam {
	int	task_prio;
	int	task_state;
	int	task_memuse;
	int	task_diskuse;
};

typedef union {
	struct resparam  res;
	struct jobparam  job;
	struct taskparam tsk;
} resource_param_t;

typedef struct jobstartinfo {
	char *ji_hname;
	int ji_sock1;
	int ji_sock2;
} start_info_t;

int resource_init __P((void));
int resource_names __P((resource_name_t **, int *));
int resource_open __P((resource_name_t, resource_id_t *));
int resource_params __P((resource_id_t, job_id_t, task_id_t, resource_param_t *, resource_param_t *));
int resource_allocate __P((resource_id_t, int, int));
int resource_activate __P((resource_id_t, int, job_id_t *, int, task_id_t *, start_info_t *));
int resource_free __P((resource_id_t));
int resource_event __P((resource_id_t, job_id_t, task_id_t, int));
int resource_close __P((resource_id_t));

#define resource_isused(r)		((r).r_pid == NO_PID)
#define resource_markunused(r)		((r).r_pid = NO_PID)
#define resource_ridcmp(r1,r2)		strcmp((r1),(r2))
#define resource_rnamecmp(r1,r2)	strcmp((r1),(r2))

#endif /* _CONDOR_CONDOR_RESOURCE_H */
