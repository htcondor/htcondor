
#ifndef _RecoveryLog_H_
#define _RecoveryLog_H_

#include "log.h"
#include "RecoveryLogConstants.h"
#include "DagMan.h"

class LogDagManInitialize : public LogRecord{
public: 
	LogDagManInitialize(char *_dataFile);
	int Play(char *input_file);
	int ReadBody(FILE *fp);

private:
	int WriteBody(FILE *fp);
		
	char dataFile[50];
};

class LogDagManDoneInitialize : public LogRecord{
public: 
	LogDagManDoneInitialize(char *_msg);
	int Play();
	int ReadBody(FILE *fp);

private:
	int WriteBody(FILE *fp);

	char msg[50];
};

class LogCondorJob : public LogRecord{
public:
	LogCondorJob(int _jobType, int _cluster, int _proc, int _subproc, int _jobId, char *_jobname); //Condor_Submit/Terminate
	int Play(CondorID *condorID, JobID& jobID);
	int ReadBody(FILE *fp);

private:
        int WriteBody(FILE *fp);

	int cluster;
	int proc;
	int subproc;
	int jobid;
	char jobname[50];

};

#endif
