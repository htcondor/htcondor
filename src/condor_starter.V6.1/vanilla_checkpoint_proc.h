#ifndef _CONDOR_VANILLA_CHECKPOINT_PROC_H
#define _CONDOR_VANILLA_CHECKPOINT_PROC_H

#include "vanilla_proc.h"
#include "vm_proc.h"

//
// For this class to make sense, some of the magic in VanillaProc::StartJob()
// will have to be split out into protected functions we can call.
//

class VanillaCheckpointProc : public VanillaProc {
	public:
		VanillaCheckpointProc( ClassAd * jobAd );
		virtual ~VanillaCheckpointProc();

		virtual int StartJob();
		virtual bool JobReaper( int pid, int status );
		virtual bool JobExit();

		virtual void Suspend();
		virtual void Continue();

		virtual bool Remove();
		virtual bool Hold();

		virtual bool Ckpt();
		virtual void CkptDone( bool success );

		virtual bool ShutdownGraceful();
		virtual bool ShutdownFast();

		virtual bool PublishUpdateAd( ClassAd * jobAd );
		virtual void PublishToEnv( Env * env );

	private:
		// TO DO.
		StarterStatistics m_statistics;

		// With no default constructor, we can't hold a VMProc directly
		// unless it really can use the same jobAd we do.
		VMProc * vmProc;

		// The base starter should provide this but doesn't.
		bool shuttingDown;

		// A convenient refactoring.
		bool CreateInitialDisks();
};

#endif /* _CONDOR_VANILLA_CHECKPOINT_PROC_H */
