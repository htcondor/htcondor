#ifndef _CONDOR_RESMGR_H
#define _CONDOR_RESMGR_H

#include "condor_classad.h"

typedef struct {
	resource_id_t r_rid;
	resource_name_t r_name;
	int r_pid;
	resource_param_t r_param;
	int r_state;
        ClassAd	*r_classad;
	ClassAd *r_jobclassad;
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
	char *r_origreqexp;
	int r_timed_out;
} resource_info_t;

int resmgr_init(void);
int resmgr_add(resource_id_t, resource_info_t *);
int resmgr_del(resource_id_t rid);
extern "C" resource_info_t *resmgr_getbyrid(resource_id_t rid);
resource_info_t *resmgr_getbyname(resource_name_t);
resource_info_t *resmgr_getbypid(int);
int resmgr_walk(int (*) (resource_info_t *));
int resmgr_vacateall(void);
bool resmgr_resourceinuse(void);
extern "C" ClassAd *resmgr_classad(resource_id_t rid);
int resource_initclassad(resource_info_t *);
ClassAd *resource_update_classad(resource_info_t *);
ClassAd *resource_timeout_classad(resource_info_t *);

#endif /* _CONDOR_RESMGR_H */
