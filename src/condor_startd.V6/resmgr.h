#ifndef _CONDOR_RESMGR_H
#define _CONDOR_RESMGR_H

typedef struct {
	resource_id_t r_rid;
	resource_name_t r_name;
	int r_pid;
	resource_param_t r_param;
	int r_state;
	CONTEXT	*r_context;
	CONTEXT *r_jobcontext;
	char *r_starter;
	int r_port;
	int r_sock;
	int r_claimed;
	int r_universe;
	char *r_client;
	char *r_user;
	char *r_clientmachine;
	char *r_capab;
	time_t r_captime;
	time_t r_receivetime;
	int r_interval;
} resource_info_t;

int resmgr_init __P((void));
int resmgr_add __P((resource_id_t, resource_info_t *));
int resmgr_del __P((resource_id_t rid));
resource_info_t *resmgr_getbyrid __P((resource_id_t));
resource_info_t *resmgr_getbyname __P((resource_name_t));
resource_info_t *resmgr_getbypid __P((int));
int resmgr_walk __P((int (*) __P((resource_info_t *))));
int resmgr_vacateall __P((void));
CONTEXT *resmgr_context __P((resource_id_t));
int resource_initcontext __P((resource_info_t *));
CONTEXT *resource_context __P((resource_info_t *));

#endif /* _CONDOR_RESMGR_H */
