#if !defined(_QMGMT_LOG_H)
#define _QMGMT_LOG_H

#include "qmgmt_constants.h"
#include "qmgmt.h"
#include "log.h"

class LogNewCluster : public LogRecord {
public:
	LogNewCluster(int cl_num = -1);
	int Play();

private:
	WriteBody(int fd) { return write(fd, &cluster_num, sizeof(cluster_num)); }
	WriteBody(XDR *xdrs) { return xdr_int(xdrs, &cluster_num); }
	ReadBody(int fd) { return read(fd, &cluster_num, sizeof(cluster_num)); }
	
	int cluster_num;
};


class LogDestroyCluster : public LogRecord {
public:
	LogDestroyCluster(int);
	int Play();

private:
	WriteBody(int fd) { return write(fd, &cluster_num, sizeof(cluster_num)); }
	WriteBody(XDR *xdrs) { return xdr_int(xdrs, &cluster_num); }
	ReadBody(int fd) { return read(fd, &cluster_num, sizeof(cluster_num)); }
	
	int cluster_num;
};


class LogNewProc : public LogRecord {
public:
	LogNewProc(int cl_num, int p_num = -1);
	int Play();

private:
	WriteBody(int fd) { return write(fd, cluster_proc, sizeof(cluster_proc)); }

	WriteBody(XDR *xdrs) { 
		if (!xdr_int(xdrs, &(cluster_proc[0]))) {
			return 0;
		}
		return xdr_int(xdrs, &(cluster_proc[1]));
	}
		               
	ReadBody(int fd) { return read(fd, &cluster_proc, sizeof(cluster_proc)); }

	int cluster_proc[2];
};


class LogDestroyProc : public LogRecord {
public:
	LogDestroyProc(int cl_num, int p_num);
	int Play();

private:
	WriteBody(int fd) { return write(fd, cluster_proc, sizeof(cluster_proc)); }

	WriteBody(XDR *xdrs) { 
		if (!xdr_int(xdrs, &(cluster_proc[0]))) {
			return 0;
		}
		return xdr_int(xdrs, &(cluster_proc[1]));
	}
		               
	ReadBody(int fd) { return read(fd, &cluster_proc, sizeof(cluster_proc)); }

	int cluster_proc[2];
};


class LogSetAttribute : public LogRecord {
public:
	LogSetAttribute(int, int, char *, char *);
	~LogSetAttribute();
	int Play();

private:
	WriteBody(int fd);
	WriteBody(XDR *xdrs);
	ReadBody(int fd);
	ReadBody(XDR *xdrs);

	int  cluster;
	int  proc;
	char *name;
	char *value;
};

class LogDeleteAttribute : public LogRecord {
public:
	LogDeleteAttribute(int, int, char *);
	~LogDeleteAttribute();
	int Play();

private:
	WriteBody(int fd);
	WriteBody(XDR *xdrs);
	ReadBody(int fd);
	ReadBody(XDR *xdrs);

	int  cluster;
	int  proc;
	char *name;
};

#endif
