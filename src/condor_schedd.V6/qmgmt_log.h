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
	int WriteBody(int fd) { return write(fd, &cluster_num, sizeof(cluster_num)); }
	int WriteBody(Stream *s) { return s->code(cluster_num); }
	int ReadBody(int fd) { return read(fd, &cluster_num, sizeof(cluster_num)); }
	
	int cluster_num;
};


class LogDestroyCluster : public LogRecord {
public:
	LogDestroyCluster(int);
	int Play();

private:
	int WriteBody(int fd) { return write(fd, &cluster_num, sizeof(cluster_num)); }
	int WriteBody(Stream *s) { return s->code(cluster_num); }
	int ReadBody(int fd) { return read(fd, &cluster_num, sizeof(cluster_num)); }
	
	int cluster_num;
};


class LogNewProc : public LogRecord {
public:
	LogNewProc(int cl_num, int p_num = -1);
	int Play();

private:
	int WriteBody(int fd) { return write(fd, cluster_proc, sizeof(cluster_proc)); }

	int WriteBody(Stream *s) { 
		if (!s->code(cluster_proc[0])) {
			return 0;
		}
		return s->code(cluster_proc[1]);
	}
		               
	int ReadBody(int fd) { return read(fd, &cluster_proc, sizeof(cluster_proc)); }

	int cluster_proc[2];
};


class LogDestroyProc : public LogRecord {
public:
	LogDestroyProc(int cl_num, int p_num);
	int Play();

private:
	int WriteBody(int fd) { return write(fd, cluster_proc, sizeof(cluster_proc)); }

	int WriteBody(Stream *s) { 
		if (!s->code(cluster_proc[0])) {
			return 0;
		}
		return s->code(cluster_proc[1]);
	}
		               
	int ReadBody(int fd) { return read(fd, &cluster_proc, sizeof(cluster_proc)); }

	int cluster_proc[2];
};


class LogSetAttribute : public LogRecord {
public:
	LogSetAttribute(int, int, const char *, char *);
	~LogSetAttribute();
	int Play();

private:
	int WriteBody(int fd);
	int WriteBody(Stream *s);
	int ReadBody(int fd);
	int ReadBody(Stream *s);

	int  cluster;
	int  proc;
	char *name;
	char *value;
};

class LogDeleteAttribute : public LogRecord {
public:
	LogDeleteAttribute(int, int, const char *);
	~LogDeleteAttribute();
	int Play();

private:
	int WriteBody(int fd);
	int WriteBody(Stream *s);
	int ReadBody(int fd);
	int ReadBody(Stream *s);

	int  cluster;
	int  proc;
	char *name;
};

#endif
