/* 
** Copyright 1996 by Miron Livny, and Jim Pruyne
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the copyright holders not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the 
** copyright holders make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE COPYRIGHT HOLDERS DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Author:  Jim Pruyne
**
*/ 

#define _POSIX_SOURCE

#include "condor_common.h"
#include "qmgmt.h"
#include "qmgmt_log.h"

LogNewCluster::LogNewCluster(int cl_num)
{
	op_type = CONDOR_NewCluster;
	body_size = sizeof(cluster_num);
	cluster_num = cl_num;
}


int
LogNewCluster::Play()
{
	Cluster	*cl;

	if (cluster_num != -1) {
		cl = new Cluster(cluster_num);
	} else {
		return -1;
	}
	return 0;
}


LogDestroyCluster::LogDestroyCluster(int cl_num)
{
	op_type = CONDOR_DestroyCluster;
	body_size = sizeof(cluster_num);
	cluster_num = cl_num;
}


int
LogDestroyCluster::Play()
{
	Cluster	*cl;

	cl = FindCluster(cluster_num);
	if (cl == 0) {
		return -1;
	} else {
		delete cl;
	}
	return 0;
}


LogNewProc::LogNewProc(int cl_num, int p_num)
{
	op_type = CONDOR_NewProc;
	body_size = sizeof(cluster_proc);
	cluster_proc[0] = cl_num;
	cluster_proc[1] = p_num;
}


int
LogNewProc::Play()
{
	Cluster	*cl;
	Job		*job;

	cl = FindCluster(cluster_proc[0]);
	if (cl == 0) {
		return -1;
	}

	job = cl->new_job(cluster_proc[1]);
	if (job == 0) {
		return -1;
	}
	return 0;
}


LogDestroyProc::LogDestroyProc(int cl_num, int p_num)
{
	op_type = CONDOR_DestroyProc;
	body_size = sizeof(cluster_proc);
	cluster_proc[0] = cl_num;
	cluster_proc[1] = p_num;
}


int
LogDestroyProc::Play()
{
	Job		*job;

	job = FindJob(cluster_proc[0], cluster_proc[1]);

	if (job == 0) {
		return -1;
	}

	delete job;
	return 0;
}

LogSetAttribute::LogSetAttribute(int cl, int p, const char *n, char *val)
{
	op_type = CONDOR_SetAttribute;
	body_size = sizeof(cluster) + sizeof(proc) + strlen(n) + 1 + 
		strlen(val) + 1;
	cluster = cl;
	proc = p;
	name = strdup(n);
	value = strdup(val);
}


LogSetAttribute::~LogSetAttribute()
{
	free(name);
	free(value);
}


int
LogSetAttribute::Play()
{
	Job	*job;

	job = FindJob(cluster, proc);
	if (job == 0) {
		return -1;
	}
	job->SetAttribute(name, value);
	return 0;
}


int
LogSetAttribute::WriteBody(int fd)
{
	int		rval, rval1;

	rval = write(fd, &cluster, sizeof(cluster));
	if (rval < 0) {
		return rval;
	}
	rval1 = write(fd, &proc, sizeof(proc));
	if (rval1 < 0) {
		return rval1;
	}
	rval1 += rval;
	rval = write(fd, name, strlen(name) + 1);
	if (rval < 0) {
		return rval;
	}
	rval1 += rval;
	rval = write(fd, value, strlen(value) + 1);
	if (rval < 0) {
		return rval;
	}
	return rval1 + rval;
}


int
LogSetAttribute::WriteBody(Stream *s)
{
	if (!s->code(cluster)) {
		return 0;
	}
	if (!s->code(proc)) {
		return 0;
	}
	if (!s->code(name)) {
		return 0;
	}
	if (!s->code(value)) {
		return 0;
	}
	return 1;
}


int
LogSetAttribute::ReadBody(int fd)
{
	int rval, rval1;

	rval = read(fd, &cluster, sizeof(cluster));
	if (rval < 0) {
		return rval;
	}
	rval1 = read(fd, &proc, sizeof(proc));
	if (rval1 < 0) {
		return rval1;
	}
	rval1 += rval;

	if (name != 0) {
		free(name);
	}
	rval = readstring(fd, name);
	if (rval < 0) {
		return rval;
	}
	rval1 += rval;

	if (value != 0) {
		free(value);
	}
	rval = readstring(fd, value);
	if (rval < 0) {
		return rval;
	}
	body_size = sizeof(cluster) + sizeof(proc) + strlen(name) + 1 + 
		strlen(value) + 1;
	return rval + rval1;
}


int
LogSetAttribute::ReadBody(Stream *s)
{
	char	buf[1000];
	int		bufsize;
	char	*ptr;

	if (!s->code(cluster)) {
		return 0;
	}
	if (!s->code(proc)) {
		return 0;
	}
	ptr = &(buf[0]);
	bufsize = sizeof(buf);
	if (!s->code(ptr, bufsize)) {
		return 0;
	}

	if (name != 0) {
		free(name);
	}
	name = strdup(buf);
	ptr = &(buf[0]);
	bufsize = sizeof(buf);
	if (!s->code(ptr, bufsize)) {
		return 0;
	}

	if (value != 0) {
		free(value);
	}
	value = strdup(buf);
	body_size = sizeof(cluster) + sizeof(proc) + strlen(name) + 1 + 
		strlen(value) + 1;
	return 1;
}


LogDeleteAttribute::LogDeleteAttribute(int cl, int p, const char *n)
{
	op_type = CONDOR_DeleteAttribute;
	body_size = sizeof(cluster) + sizeof(proc) + strlen(n) + 1;
	cluster = cl;
	proc = p;
	name = strdup(n);
}


LogDeleteAttribute::~LogDeleteAttribute()
{
	free(name);
}


int
LogDeleteAttribute::Play()
{
	Job	*job;

	job = FindJob(cluster, proc);
	if (job == 0) {
		return -1;
	}
	return job->DeleteAttribute(name);
}


int
LogDeleteAttribute::WriteBody(int fd)
{
	int		rval, rval1;

	rval = write(fd, &cluster, sizeof(cluster));
	if (rval < 0) {
		return rval;
	}
	rval1 = write(fd, &proc, sizeof(proc));
	if (rval1 < 0) {
		return rval1;
	}
	rval1 += rval;
	rval = write(fd, name, strlen(name) + 1);
	if (rval < 0) {
		return rval;
	}
	return rval1 + rval;
}


int
LogDeleteAttribute::WriteBody(Stream *s)
{
	if (!s->code(cluster)) {
		return 0;
	}
	if (!s->code(proc)) {
		return 0;
	}
	if (!s->code(name)) {
		return 0;
	}
	return 1;
}


int
LogDeleteAttribute::ReadBody(int fd)
{
	int rval, rval1;

	rval = read(fd, &cluster, sizeof(cluster));
	if (rval < 0) {
		return rval;
	}
	rval1 = read(fd, &proc, sizeof(proc));
	if (rval1 < 0) {
		return rval1;
	}
	rval1 += rval;

	if (name != 0) {
		free(name);
	}
	rval = readstring(fd, name);
	if (rval < 0) {
		return rval;
	}
	body_size = sizeof(cluster) + sizeof(proc) + strlen(name) + 1;
	return rval + rval1;
}


int
LogDeleteAttribute::ReadBody(Stream *s)
{
	char	buf[1000];
	char	*ptr;
	int		bufsize = sizeof(buf);

	if (!s->code(cluster)) {
		return 0;
	}
	if (!s->code(proc)) {
		return 0;
	}
	ptr = &(buf[0]);
	if (!s->code(ptr, bufsize)) {
		return 0;
	}

	if (name != 0) {
		free(name);
	}
	name = strdup(buf);

	body_size = sizeof(cluster) + sizeof(proc) + strlen(name) + 1;
	return 1;
}


LogRecord	*
InstantiateLogEntry(int fd, int type)
{
	LogRecord	*log_rec;

	switch(type) {
	    case CONDOR_NewCluster:
		    log_rec = new LogNewCluster(-1);
			break;
	    case CONDOR_NewProc:
		    log_rec = new LogNewProc(-1, -1);
			break;
	    case CONDOR_DestroyCluster:
		    log_rec = new LogDestroyCluster(-1);
			break;
	    case CONDOR_DestroyProc:
		    log_rec = new LogDestroyProc(-1, -1);
			break;
	    case CONDOR_SetAttribute:
		    log_rec = new LogSetAttribute(-1, -1, "", "");
			break;
	    case CONDOR_DeleteAttribute:
		    log_rec = new LogDeleteAttribute(-1, -1, "");
			break;
	    default:
		    return 0;
			break;
	}
	log_rec->ReadBody(fd);
	return log_rec;
}
