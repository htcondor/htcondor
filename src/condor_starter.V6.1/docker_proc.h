#ifndef _CONDOR_DOCKER_PROC_H
#define _CONDOR_DOCKER_PROC_H

#include "vanilla_proc.h"

class DockerProc : public VanillaProc {
	public:
		DockerProc( ClassAd * jobAd );
		virtual ~DockerProc();

		virtual int StartJob();
		virtual bool JobReaper( int pid, int status );
		virtual int ExecReaper( int pid, int status );
		virtual bool JobExit();

		virtual void Suspend();
		virtual void Continue();

		virtual bool Remove();
		virtual bool Hold();

		// For when Docker's CRiU actually does something useful.
		// You'll also need to turn periodic checkpoints on in the rest of
		// the code; see V8_1-gittrac_4297-branch or V8_1-vc-branch.
		// virtual bool Ckpt();
		// virtual void CpktDone();

		virtual bool ShutdownGraceful();
		virtual bool ShutdownFast();

		virtual bool PublishUpdateAd( ClassAd * jobAd );
		virtual void PublishToEnv( Env * env );

		virtual void SetupDockerSsh();
		virtual int  AcceptSSHClient(Stream *stream);

		virtual void getStats( int timerID = -1 );
		static bool Detect();
		static bool Version( std::string & version );

	protected:
		virtual void restartCheckpointedJob();
	private:

		ReliSock listener;
		std::string containerID;
		std::string containerName;
		std::string imageName;
		int updateTid;
		uint64_t memUsage, max_memUsage, netIn, netOut, userCpu, sysCpu;
		bool waitForCreate;
		int execReaperId;

		bool shouldAskForServicePorts;
		ClassAd serviceAd;
};

#endif /* _CONDOR_DOCKER_PROC_H */
