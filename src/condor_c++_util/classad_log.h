#if !defined(_QMGMT_LOG_H)
#define _QMGMT_LOG_H

#include "log.h"
#include "classad_hashtable.h"
#include "log_transaction.h"

#define CondorLogOp_NewClassAd			101
#define CondorLogOp_DestroyClassAd		102
#define CondorLogOp_SetAttribute		103
#define CondorLogOp_DeleteAttribute		104
#define CondorLogOp_BeginTransaction	105
#define CondorLogOp_EndTransaction		106

class ClassAdLog {
public:
	ClassAdLog();
	ClassAdLog(const char *filename);
	~ClassAdLog();

	void AppendLog(LogRecord *log);
	void TruncLog();

	void BeginTransaction();
	void AbortTransaction();
	void CommitTransaction();

	// returns 1 and sets val if corresponding SetAttribute found
	// returns 0 if no SetAttribute found
	// return -1 if DeleteAttribute or DestroyClassAd found
	int LookupInTransaction(const char *key, const char *name, char *&val);

	ClassAdHashTable table;
private:
	void LogState(int fd);
	char log_filename[_POSIX_PATH_MAX];
	int log_fd;
	Transaction *active_transaction;
};

class LogNewClassAd : public LogRecord {
public:
	LogNewClassAd(const char *key, char *mytype, char *targettype);
	~LogNewClassAd();
	int Play(ClassAdHashTable &table);
	char *get_key() { return strdup(key); }
	char *get_mytype() { return strdup(mytype); }
	char *get_targettype() { return strdup(targettype); }

private:
	virtual int WriteBody(int fd);
	virtual int ReadBody(int fd);

	char *key;
	char *mytype;
	char *targettype;
};


class LogDestroyClassAd : public LogRecord {
public:
	LogDestroyClassAd(const char *key);
	~LogDestroyClassAd();
	int Play(ClassAdHashTable &table);
	char *get_key() { return strdup(key); }

private:
	virtual int WriteBody(int fd) { return write(fd, key, strlen(key)); }
	virtual int ReadBody(int fd);

	char *key;
};


class LogSetAttribute : public LogRecord {
public:
	LogSetAttribute(const char *key, const char *name, char *value);
	~LogSetAttribute();
	int Play(ClassAdHashTable &table);
	char *get_key() { return strdup(key); }
	char *get_name() { return strdup(name); }
	char *get_value() { return strdup(value); }

private:
	virtual int WriteBody(int fd);
	virtual int ReadBody(int fd);

	char *key;
	char *name;
	char *value;
};

class LogDeleteAttribute : public LogRecord {
public:
	LogDeleteAttribute(const char *key, const char *name);
	~LogDeleteAttribute();
	int Play(ClassAdHashTable &table);
	char *get_key() { return strdup(key); }
	char *get_name() { return strdup(name); }

private:
	virtual int WriteBody(int fd);
	virtual int ReadBody(int fd);

	char *key;
	char *name;
};

class LogBeginTransaction : public LogRecord {
public:
	LogBeginTransaction() { op_type = CondorLogOp_BeginTransaction; }
private:
	virtual int WriteBody(int fd) { return 0; }
	virtual int ReadBody(int fd) { return 0; }
};

class LogEndTransaction : public LogRecord {
public:
	LogEndTransaction() { op_type = CondorLogOp_EndTransaction; }
private:
	virtual int WriteBody(int fd) { return 0; }
	virtual int ReadBody(int fd) { return 0; }
};

#endif
