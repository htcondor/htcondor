
#include "condor_common.h"
#include "RecoveryLog.h"

LogDagManInitialize::LogDagManInitialize(char *_dataFile)
{
	op_type = DAGMAN_Initialize;
	body_size = 0;  //need to find out if this has to be changed!
	strcpy(dataFile,_dataFile);
}

int
LogDagManInitialize::WriteBody(FILE *fp)
{
	int rval, tot;

	rval = fprintf(fp, "%s", dataFile);
	if(rval < 0){
		return rval;
	}
	tot = rval;
	return tot;
}

int
LogDagManInitialize::ReadBody(FILE *fp)
{
	int rval;

	rval = fscanf(fp,"%s",dataFile);
	return rval;
}

int 
LogDagManInitialize::Play(char *input_file)
{
	if(strcmp(input_file,dataFile) == 0)
		return RECOVER;
	else 
		return NORECOVER;
}

LogDagManDoneInitialize::LogDagManDoneInitialize(char *_msg)
{
	op_type = DAGMAN_DoneInitialize;
	body_size = 0;
	strcpy(msg,_msg);
}

int
LogDagManDoneInitialize::WriteBody(FILE *fp)
{
	int rval;
	rval = fprintf(fp, "%s", msg);
	return rval;
}

int
LogDagManDoneInitialize::ReadBody(FILE *fp)
{
	int rval;
	rval = fscanf(fp,"%s",msg);
	
}

int
LogDagManDoneInitialize::Play()
{
	if(strcmp(msg,"DoneInitializingDagMan")==0)
		return RECOVER;
	else
		return NORECOVER;
}


LogCondorJob::LogCondorJob(int _jobtype,int _cluster,int _proc,int _subproc,int _jobid,char *_jobname)
{
	op_type = _jobtype;
	body_size = 0;
	cluster = _cluster;
	proc = _proc;
	subproc = _subproc;
	jobid = _jobid;
	strcpy(jobname,_jobname);
}

int
LogCondorJob::WriteBody(FILE *fp)
{
	int tot,rval;

	rval = fprintf(fp,"%d %s %d %d %d",jobid,jobname,cluster,proc,subproc);
	return rval;
}

int
LogCondorJob::ReadBody(FILE *fp)
{
	int rval;

	rval = fscanf(fp,"%d %s %d %d %d",&jobid,jobname,&cluster,&proc,&subproc);

	return rval;
}

int 
LogCondorJob::Play(CondorID *condorID, JobID& jobID)
{
	condorID->cluster = cluster;
	condorID->proc = proc;
	condorID->subproc = subproc;
	jobID = jobid;
	return op_type;
}

LogRecord*
InstantiateLogEntry(FILE *fp, int type)
{
	LogRecord *log_rec;

	switch(type){
		case DAGMAN_Initialize:
			log_rec = new LogDagManInitialize("");
			break;
		case DAGMAN_DoneInitialize:
			log_rec = new LogDagManDoneInitialize("");
			break;
		case CONDOR_SubmitJob:
			log_rec = new LogCondorJob(-1,-1,-1,-1,-1,"");
			break;
		default:
			return 0;
			break;
	}
	log_rec->ReadBody(fp);
	return log_rec;
}

LogRecord*
InstantiateLogEntry(int fd, int type)
{
	LogRecord *log_rec;

	switch(type){
		case DAGMAN_Initialize:
			log_rec = new LogDagManInitialize("");
			break;
		case DAGMAN_DoneInitialize:
			log_rec = new LogDagManDoneInitialize("");
			break;
		case CONDOR_SubmitJob:
			log_rec = new LogCondorJob(-1,-1,-1,-1,-1,"");
			break;
		default:
			return 0;
			break;
	}
	log_rec->ReadBody(fd);
	return log_rec;
}

