#define _POSIX_SOURCE
#include "condor_common.h"
#include <pwd.h>
/*
#include "condor_xdr.h"
*/
#include "condor_io.h"

#include "qmgr.h"
#include "condor_qmgr.h"
#include "condor_debug.h"
#include "condor_attributes.h"

int open_url(char *, int, int);
extern "C" int gethostname(char *name, int namelen);
extern "C" char*	get_schedd_addr(const char*); 

static char *_FileName_ = __FILE__;

/*
XDR *xdr_qmgmt = 0;
*/
ReliSock *qmgmt_sock;
static Qmgr_connection *connection = 0;

Qmgr_connection *
ConnectQ(char *qmgr_location)
{
	char	localurl[1000];
/*	XDR		*xdrs;*/
	int		rval;
	char	owner[100];
	char	tmp_file[255];
	int		fd;
	int		cmd;
	struct  passwd *pwd;
	char	hostname[256]; 
	char*	scheddAddr = NULL;

	if (connection != 0) {
		connection->count++;
		return connection;
	}

	connection = (Qmgr_connection *) malloc(sizeof(Qmgr_connection));
	if (connection == 0) {
		return 0;
	}
	connection->rendevous_file = 0;

	/* No schedd identified --- use local schedd */
	if (qmgr_location == 0) {
		if(gethostname(hostname, 256) < 0)
		{
			dprintf(D_ALWAYS, "Can't find host name");
			return 0;
		}
		scheddAddr = get_schedd_addr(hostname); 
	}
	/* schedd identified by Name, not <xxx.xxx.xxx.xxx:xxxx> */
	else if(qmgr_location[0] != '<')
	{
		/* get schedd's IP address from collector */
		scheddAddr = get_schedd_addr(qmgr_location);
	}

	if(scheddAddr)
	{
		qmgmt_sock = new ReliSock(scheddAddr, QMGR_PORT);
		free(scheddAddr);
	}
	else if(qmgr_location)
	{
		qmgmt_sock = new ReliSock(qmgr_location, QMGR_PORT);
	}
	else
	{
		dprintf(D_ALWAYS, "Can't connect to qmgr\n");
		free(connection);
		return 0;
	}
	connection->fd = qmgmt_sock->get_file_desc();
	if (connection->fd < 0) {
		dprintf(D_ALWAYS, "Can't connect to qmgr\n");
		free(connection);
		return 0;
	}
/*	xdrs = xdr_Init( &(connection->fd), &(connection->xdr));
	xdr_qmgmt = xdrs; */
/*	xdrrec_endofrecord(xdr_qmgmt,TRUE); */

	/* Get the schedd to handle Q ops. */
	qmgmt_sock->encode();
	cmd = QMGMT_CMD;
	qmgmt_sock->code(cmd);
	qmgmt_sock->end_of_message();

	pwd = getpwuid(geteuid());
	if (pwd == 0) {
		free(connection);
		return 0;
	}
	rval = InitializeConnection(pwd->pw_name, tmp_file);
	if (rval < 0) {
		free(connection);
		return 0;
	}
	fd = open(tmp_file, O_RDONLY | O_CREAT | O_TRUNC, 0666);
	close(fd);
	connection->count = 1;
	connection->rendevous_file = strdup(tmp_file);
	return connection;
}


DisconnectQ(Qmgr_connection *conn)
{
	if (conn == 0) {
		conn = connection;
	}
	conn->count--;

	if (conn->count == 0) {
		CloseConnection();
		delete qmgmt_sock;
		qmgmt_sock = NULL;
		close(conn->fd);
		if (conn->rendevous_file != 0) {
			unlink(conn->rendevous_file);
			free(conn->rendevous_file);
			conn->rendevous_file = 0;
		}
		free(conn);
		if (conn == connection) {
			connection = 0;
		}
	}
}



WalkJobQueue(scan_func func)
{
	int	cluster, proc;
	int next_cluster, next_proc;
	int disconn_when_done = 0;
	int rval;

#if 0
	if (connection == 0) {
		disconn_when_done = 1;
		ConnectQ();
		if (connection == 0) {
			return -1;
		}
	}
#endif

	cluster = -1;
	proc = -1;

	do {
		rval = GetNextJob(cluster, proc, &next_cluster, &next_proc);
		if (rval >= 0) {
			cluster = next_cluster;
			proc = next_proc;
			rval = func(cluster, proc);
		} else {
			cluster = -1;
		}
	} while (rval != -1);

	if (disconn_when_done) {
		DisconnectQ(connection);
	}
}


float
rusage_to_float(struct rusage ru)
{
	float rval;

	rval = (float) (ru.ru_utime.tv_sec + ru.ru_stime.tv_sec);
	return rval;
}


float_to_rusage(float cpu_time, struct rusage *ru)
{
	ru->ru_utime.tv_sec = (int) cpu_time;
	return 0;
}


int
SaveProc(PROC *p)
{
	int disconn_when_done = 0;
	char buf[1000];
	int cl;
	int pr;

	if (connection == 0) {
		disconn_when_done = 1;
		ConnectQ(0);
		if (connection == 0) {
			return -1;
		}
	}

	cl = p->id.cluster;
	pr = p->id.proc;
	SetAttributeInt(cl, pr, ATTR_JOB_UNIVERSE, p->universe);
	SetAttributeInt(cl, pr, ATTR_WANT_CHECKPOINT, p->checkpoint);
	SetAttributeInt(cl, pr, ATTR_WANT_REMOTE_SYSCALLS, p->remote_syscalls);
	SetAttributeString(cl, pr, ATTR_OWNER, p->owner);
	SetAttributeInt(cl, pr, ATTR_Q_DATE, p->q_date);
	SetAttributeInt(cl, pr, ATTR_COMPLETION_DATE, p->completion_date);
	SetAttributeInt(cl, pr, ATTR_JOB_STATUS, p->status);
	SetAttributeInt(cl, pr, ATTR_JOB_PRIO, p->prio);
	SetAttributeInt(cl, pr, ATTR_JOB_NOTIFICATION, p->notification);
	SetAttributeInt(cl, pr, ATTR_IMAGE_SIZE, p->image_size);
	SetAttributeString(cl, pr, ATTR_JOB_ENVIRONMENT, p->env);

	SetAttributeString(cl, pr, ATTR_JOB_CMD, p->cmd[0]);
	SetAttributeString(cl, pr, ATTR_JOB_ARGUMENTS, p->args[0]);
	SetAttributeString(cl, pr, ATTR_JOB_INPUT, p->in[0]);
	SetAttributeString(cl, pr, ATTR_JOB_OUTPUT, p->out[0]);
	SetAttributeString(cl, pr, ATTR_JOB_ERROR, p->err[0]);
	SetAttributeInt(cl, pr, ATTR_JOB_EXIT_STATUS, p->exit_status[0]);

	SetAttributeInt(cl, pr, ATTR_CURRENT_HOSTS,(p->min_needed >> 16) & 0xffff);
	SetAttributeInt(cl, pr, ATTR_MIN_HOSTS, (p->min_needed & 0xffff));
	SetAttributeInt(cl, pr, ATTR_MAX_HOSTS, p->max_needed);

	SetAttributeString(cl, pr, ATTR_JOB_ROOT_DIR, p->rootdir);
	SetAttributeString(cl, pr, ATTR_JOB_IWD, p->iwd);
	SetAttributeExpr(cl, pr, ATTR_REQUIREMENTS, p->requirements);
	if (*(p->preferences) == '\0') {
		strcpy(p->preferences, "TRUE");
	}
	SetAttributeExpr(cl, pr, ATTR_PREFERENCES, p->preferences);
	
	SetAttributeFloat(cl, pr, ATTR_JOB_LOCAL_CPU,
					  rusage_to_float(p->local_usage));
	SetAttributeFloat(cl, pr, ATTR_JOB_REMOTE_CPU, 
					  rusage_to_float(p->remote_usage[0]));
	

	if (disconn_when_done) {
		DisconnectQ(connection);
	}
	return 0;
}




int
GetProc(int cl, int pr, PROC *p)
{
	int disconn_when_done = 0;
	char buf[1000];
	float	cpu_time;
	char	*s;

	if (connection == 0) {
		disconn_when_done = 1;
		ConnectQ(0);
		if (connection == 0) {
			return -1;
		}
	}

	p->version_num = 3;
	p->id.cluster = cl;
	p->id.proc = pr;

	GetAttributeInt(cl, pr, ATTR_JOB_UNIVERSE, &(p->universe));
	GetAttributeInt(cl, pr, ATTR_WANT_CHECKPOINT, &(p->checkpoint));
	GetAttributeInt(cl, pr, ATTR_WANT_REMOTE_SYSCALLS, &(p->remote_syscalls));
	GetAttributeString(cl, pr, ATTR_OWNER, buf);
	p->owner = strdup(buf);
	GetAttributeInt(cl, pr, ATTR_Q_DATE, &(p->q_date));
	GetAttributeInt(cl, pr, ATTR_COMPLETION_DATE, &(p->completion_date));
	GetAttributeInt(cl, pr, ATTR_JOB_STATUS, &(p->status));
	GetAttributeInt(cl, pr, ATTR_JOB_PRIO, &(p->prio));
	GetAttributeInt(cl, pr, ATTR_JOB_NOTIFICATION, &(p->notification));
	GetAttributeInt(cl, pr, ATTR_IMAGE_SIZE, &(p->image_size));
	GetAttributeString(cl, pr, ATTR_JOB_ENVIRONMENT, buf);
	p->env = strdup(buf);

	p->n_cmds = 1;
	p->cmd = (char **) malloc(p->n_cmds * sizeof(char *));
	p->args = (char **) malloc(p->n_cmds * sizeof(char *));
	p->in = (char **) malloc(p->n_cmds * sizeof(char *));
	p->out = (char **) malloc(p->n_cmds * sizeof(char *));
	p->err = (char **) malloc(p->n_cmds * sizeof(char *));
	p->exit_status = (int *) malloc(p->n_cmds * sizeof(int));

	GetAttributeString(cl, pr, ATTR_JOB_CMD, buf);
	p->cmd[0] = strdup(buf);
	GetAttributeString(cl, pr, ATTR_JOB_ARGUMENTS, buf);
	p->args[0] = strdup(buf);
	GetAttributeString(cl, pr, ATTR_JOB_INPUT, buf);
	p->in[0] = strdup(buf);
	GetAttributeString(cl, pr, ATTR_JOB_OUTPUT, buf);
	p->out[0] = strdup(buf);
	GetAttributeString(cl, pr, ATTR_JOB_ERROR, buf);
	p->err[0] = strdup(buf);
	GetAttributeInt(cl, pr, ATTR_JOB_EXIT_STATUS, &(p->exit_status[0]));

	GetAttributeInt(cl, pr, ATTR_MIN_HOSTS, &(p->min_needed));
	GetAttributeInt(cl, pr, ATTR_MAX_HOSTS, &(p->max_needed));

	GetAttributeString(cl, pr, ATTR_JOB_ROOT_DIR, buf);
	p->rootdir = strdup(buf);
	GetAttributeString(cl, pr, ATTR_JOB_IWD, buf);
	p->iwd = strdup(buf);
	GetAttributeExpr(cl, pr, ATTR_REQUIREMENTS, buf);
	s = strchr(buf, '=');
	if (s) {
		s++;
		p->requirements = strdup(s);
	} else {
		p->requirements = strdup(buf);
	}
	GetAttributeExpr(cl, pr, ATTR_PREFERENCES, buf);
	s = strchr(buf, '=');
	if (s) {
		s++;
		p->preferences = strdup(s);
	} else {
		p->preferences = strdup(buf);
	}

	GetAttributeFloat(cl, pr, ATTR_JOB_LOCAL_CPU, &cpu_time);
	float_to_rusage(cpu_time, &(p->local_usage));
	p->remote_usage = (struct rusage *) malloc(p->n_cmds * 
											   sizeof(struct rusage));
	GetAttributeFloat(cl, pr, ATTR_JOB_REMOTE_CPU, &cpu_time);
	float_to_rusage(cpu_time, &(p->remote_usage[0]));
	

	if (disconn_when_done) {
		DisconnectQ(connection);
	}
	return 0;
}
