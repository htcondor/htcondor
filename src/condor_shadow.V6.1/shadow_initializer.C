#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_qmgr.h"		 // need to talk to schedd's qmgr
#include "condor_attributes.h"   // for ATTR_ ClassAd stuff
#include "internet.h"			// sinful->hostname stuff
#include "daemon.h"

#include "shadow_initializer.h"
#include "baseshadow.h"
#include "shadow.h"
#include "mpishadow.h"

ShadowInitializer::ShadowInitializer(int argc, char **argv) :
	m_jobAd(NULL), m_argc(argc), m_argv(argv), m_accept_id(-1), m_acquire_id(-1)
{
}

/* This function is here to allow developers to nicely create a list of things
	they would like done before the Shadow object gets initialized. Right
	now it is a little sparse, but it is here for expansion purposes. */
void ShadowInitializer::Bootstrap(void)
{
	InitJobAd(); /* Asynchronous */
}

/* Set up the command handler and timer to get the job ad in the best way
	possible from the schedd. */
void ShadowInitializer::InitJobAd(void)
{
	/* these callbacks I'm setting up here ultimately end up calling 
		ShadowInitialize() so the actual shadow objects gets created and 
		set up eventually. */

	/* I am guaranteed by daemon core that both of these will be registered
		before either callback gets activated */

	/* turn this on when it works */
/*	m_accept_id = daemonCore->*/
/*		Register_Command( RECEIVE_JOBAD, "RECEIVE_JOBAD",*/
/*			(CommandHandlercpp)&ShadowInitializer::AcceptJobAdFromSchedd,*/
/*			"ShadowInitializer::AcceptJobAdFromSchedd", this, READ);*/

	/* for now, wait 1 seconds and then get the job ad manually */
	m_acquire_id = daemonCore ->
		Register_Timer( 1, 0,
			(TimerHandlercpp)&ShadowInitializer::AcquireJobAdFromSchedd,
			"ShadowInitializer::AcquireJobAdFromSchedd", this);
}

/* This function receives a job ad from the schedd */
int ShadowInitializer::AcceptJobAdFromSchedd(int command, Stream *s)
{
	EXCEPT("ShadowInitializer::AcceptJobAdFromSchedd() TODO!\n");
	return FALSE;
}

/* Call back to the schedd to initialize the job ad */
int ShadowInitializer::AcquireJobAdFromSchedd(void)
{
	char *schedd_addr;
	int cluster, proc;

	/* See if the jobad was already gotten, if so, just do nothing */
	if (m_jobAd != NULL)
	{
		return TRUE;
	}

	dprintf(D_ALWAYS, "Getting JobAd from schedd manually.\n");

	schedd_addr = strdup(m_argv[1]);
	cluster = atoi(m_argv[4]);
	proc = atoi(m_argv[5]);

		// talk to the schedd to get job ad & set remote host:
	if (!ConnectQ(schedd_addr, SHADOW_QMGMT_TIMEOUT, true)) {
		free(schedd_addr);
		EXCEPT("Failed to connect to schedd!");
	}
	m_jobAd = GetJobAd(cluster, proc);
	DisconnectQ(NULL);
	if (!m_jobAd) {
		free(schedd_addr);
		EXCEPT("Failed to get job ad from schedd.");
	}
	free(schedd_addr);

	dprintf(D_ALWAYS, "Success in retreving JobAd.\n");
	ShadowInitialize();

	return TRUE;
}

void ShadowInitializer::ShadowInitialize(void)
{
	int universe; 

	ASSERT(m_jobAd != NULL);

	// For debugging, see if there's a special attribute in the
	// job ad that sends us into an infinite loop, waiting for
	// someone to attach with a debugger
	int shadow_should_wait = 0;
	m_jobAd->LookupInteger( ATTR_SHADOW_WAIT_FOR_DEBUG,
							shadow_should_wait );
	if( shadow_should_wait ) {
		dprintf( D_ALWAYS, "Job requested shadow should wait for "
			"debugger with %s=%d, going into infinite loop\n",
			ATTR_SHADOW_WAIT_FOR_DEBUG, shadow_should_wait );
		while( shadow_should_wait );
	}

	if (m_jobAd->LookupInteger(ATTR_JOB_UNIVERSE, universe) < 0) {
		// change to the right universes when everything works.
		universe = CONDOR_UNIVERSE_VANILLA;
	}

	dprintf( D_ALWAYS, "Initializing a %s shadow\n", 
			 CondorUniverseName(universe) );

	switch ( universe ) {
	case CONDOR_UNIVERSE_VANILLA:
	case CONDOR_UNIVERSE_JAVA:
		Shadow = new UniShadow();
		break;
	case CONDOR_UNIVERSE_MPI:
		Shadow = new MPIShadow();
		break;
	case CONDOR_UNIVERSE_PVM:
			// some day we'll support this.  for now, fall through and
			// print out an error message that might mean something to
			// our user, not "PVM...hopefully one day..."
//		Shadow = new PVMShadow();
//		break;
	default:
		dprintf( D_ALWAYS, "This version of the shadow cannot support "
				 "universe %d (%s)\n", universe,
				 CondorUniverseName(universe) );
		EXCEPT( "Universe not supported" );
	}
	
	Shadow->init( m_jobAd, m_argv[1], m_argv[2], m_argv[3], m_argv[4], 
				  m_argv[5] ); 
}


