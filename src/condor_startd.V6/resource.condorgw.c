#include "resource.h"

int
resource_names(rnames, nres)
	resource_name_t *rnames;
	int *nres;
{
	return 0;
}

int
resource_open(name, id)
	resource_name_t *name;
	resource_id_t *id;
{
	return 0;
}

int
resource_params(rid, jid, tid, old, new)
	resource_id_t rid;
	job_id_t jib;
	task_id_t tid;
	rparam_t *old, *new;
{
	return 0;
}

int
resource_allocate(rid, njobs, ntasks)
	resource_id_t rid;
	int njobs, ntasks;
{
	return 0;
}

int
resource_allocate(rid, njobs, ntasks)
	resource_id_ rid;
	int njobs, ntasks;
{
	return 0;
}

int
resource_activate(rid, njobs, jobs, ntasks, tasks, jobinfo)
	resource_id_t rid;
	int njobs;
	job_id_t *jobs;
	int ntasks;
	task_id_t *tasks;
	void *jobinfo;
{
	return 0;
}

int
resource_free(rid)
	resource_id_t rid;
{
	return 0;
}

int
resource_event(rid, jid, tid, ev)
	resource_id_t rid;
	jid_t jid;
	task_id_t tid;
	event_t ev;
{
	return 0;
}

int
resource_close(rid)
	resource_id_t rid;
{
	return 0;
}
