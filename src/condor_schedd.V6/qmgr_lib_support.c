#define _POSIX_SOURCE
#include "condor_common.h"
#include <pwd.h>
#include "condor_xdr.h"

#include "qmgr.h"
#include "condor_qmgr.h"

int open_url(char *, int, int);


XDR *xdr_qmgmt = 0;
static Qmgr_connection *connection = 0;

Qmgr_connection *
ConnectQ(char *qmgr_location)
{
	char	localurl[1000];
	XDR		*xdrs;
	int		rval;
	char	owner[100];
	char	tmp_file[255];
	int		fd;
	int		cmd;
	struct  passwd *pwd;

	if (connection != 0) {
		connection->count++;
		return connection;
	}

	connection = (Qmgr_connection *) malloc(sizeof(Qmgr_connection));
	if (connection == 0) {
		return 0;
	}
	connection->rendevous_file = 0;

	if (qmgr_location == 0) {
		qmgr_location = "localhost";
	}
	connection->fd = do_connect(qmgr_location, "condor_schedd", QMGR_PORT);
	if (connection->fd < 0) {
		perror("Connecting to qmgr:");
		free(connection);
		return 0;
	}
	xdrs = xdr_Init( &(connection->fd), &(connection->xdr));
	xdr_qmgmt = xdrs;
/*	xdrrec_endofrecord(xdr_qmgmt,TRUE); */

	/* Get the schedd to handle Q ops. */
	xdr_qmgmt->x_op = XDR_ENCODE;
	cmd = QMGMT_CMD;
	xdr_int(xdr_qmgmt, &cmd);
	xdrrec_endofrecord(xdr_qmgmt,TRUE);

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
		xdr_destroy(xdr_qmgmt);
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
	SetAttributeInt(cl, pr, "Universe", p->universe);
	SetAttributeInt(cl, pr, "Checkpoint", p->checkpoint);
	SetAttributeInt(cl, pr, "Remote_syscalls", p->remote_syscalls);
	SetAttributeString(cl, pr, "Owner", p->owner);
	SetAttributeInt(cl, pr, "Q_date", p->q_date);
	SetAttributeInt(cl, pr, "Completion_date", p->completion_date);
	SetAttributeInt(cl, pr, "Status", p->status);
	SetAttributeInt(cl, pr, "Prio", p->prio);
	SetAttributeInt(cl, pr, "Notification", p->notification);
	SetAttributeInt(cl, pr, "Image_size", p->image_size);
	SetAttributeString(cl, pr, "Env", p->env);

	SetAttributeString(cl, pr, "Cmd", p->cmd[0]);
	SetAttributeString(cl, pr, "Args", p->args[0]);
	SetAttributeString(cl, pr, "In", p->in[0]);
	SetAttributeString(cl, pr, "Out", p->out[0]);
	SetAttributeString(cl, pr, "Err", p->err[0]);
	SetAttributeInt(cl, pr, "Exit_status", p->exit_status[0]);

	SetAttributeInt(cl, pr, "CurrentHosts", (p->min_needed >> 16) & 0xffff);
	SetAttributeInt(cl, pr, "MinHosts", (p->min_needed & 0xffff));
	SetAttributeInt(cl, pr, "MaxHosts", p->max_needed);

	SetAttributeString(cl, pr, "Rootdir", p->rootdir);
	SetAttributeString(cl, pr, "Iwd", p->iwd);
	SetAttributeExpr(cl, pr, "Requirements", p->requirements);
	if (*(p->preferences) == '\0') {
		strcpy(p->preferences, "TRUE");
	}
	SetAttributeExpr(cl, pr, "Preferences", p->preferences);
	
	SetAttributeFloat(cl, pr, "Local_CPU", rusage_to_float(p->local_usage));
	SetAttributeFloat(cl, pr, "Remote_CPU", 
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

	GetAttributeInt(cl, pr, "Universe", &(p->universe));
	GetAttributeInt(cl, pr, "Checkpoint", &(p->checkpoint));
	GetAttributeInt(cl, pr, "Remote_syscalls", &(p->remote_syscalls));
	GetAttributeString(cl, pr, "Owner", buf);
	p->owner = strdup(buf);
	GetAttributeInt(cl, pr, "Q_date", &(p->q_date));
	GetAttributeInt(cl, pr, "Completion_date", &(p->completion_date));
	GetAttributeInt(cl, pr, "Status", &(p->status));
	GetAttributeInt(cl, pr, "Prio", &(p->prio));
	GetAttributeInt(cl, pr, "Notification", &(p->notification));
	GetAttributeInt(cl, pr, "Image_size", &(p->image_size));
	GetAttributeString(cl, pr, "Env", buf);
	p->env = strdup(buf);

	p->n_cmds = 1;
	p->cmd = (char **) malloc(p->n_cmds * sizeof(char *));
	p->args = (char **) malloc(p->n_cmds * sizeof(char *));
	p->in = (char **) malloc(p->n_cmds * sizeof(char *));
	p->out = (char **) malloc(p->n_cmds * sizeof(char *));
	p->err = (char **) malloc(p->n_cmds * sizeof(char *));
	p->exit_status = (int *) malloc(p->n_cmds * sizeof(int));

	GetAttributeString(cl, pr, "Cmd", buf);
	p->cmd[0] = strdup(buf);
	GetAttributeString(cl, pr, "Args", buf);
	p->args[0] = strdup(buf);
	GetAttributeString(cl, pr, "In", buf);
	p->in[0] = strdup(buf);
	GetAttributeString(cl, pr, "Out", buf);
	p->out[0] = strdup(buf);
	GetAttributeString(cl, pr, "Err", buf);
	p->err[0] = strdup(buf);
	GetAttributeInt(cl, pr, "Exit_status", &(p->exit_status[0]));

	GetAttributeInt(cl, pr, "MinHosts", &(p->min_needed));
	GetAttributeInt(cl, pr, "MaxHosts", &(p->max_needed));

	GetAttributeString(cl, pr, "Rootdir", buf);
	p->rootdir = strdup(buf);
	GetAttributeString(cl, pr, "Iwd", buf);
	p->iwd = strdup(buf);
	GetAttributeExpr(cl, pr, "Requirements", buf);
	s = strchr(buf, '=');
	if (s) {
		s++;
		p->requirements = strdup(s);
	} else {
		p->requirements = strdup(buf);
	}
	GetAttributeExpr(cl, pr, "Preferences", buf);
	s = strchr(buf, '=');
	if (s) {
		s++;
		p->preferences = strdup(s);
	} else {
		p->preferences = strdup(buf);
	}

	GetAttributeFloat(cl, pr, "Local_CPU", &cpu_time);
	float_to_rusage(cpu_time, &(p->local_usage));
	p->remote_usage = (struct rusage *) malloc(p->n_cmds * 
											   sizeof(struct rusage));
	GetAttributeFloat(cl, pr, "Remote_CPU", &cpu_time);
	float_to_rusage(cpu_time, &(p->remote_usage[0]));
	

	if (disconn_when_done) {
		DisconnectQ(connection);
	}
	return 0;
}
