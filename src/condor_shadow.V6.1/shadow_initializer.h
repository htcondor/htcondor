#ifndef SHADOW_INITIALIZER
#define SHADOW_INITIALIZER

#include "condor_common.h"

/* This class does two things, first, it contacts the schedd during the
	shadow initialization phase, gets the jobad, and then instantiates the
	particular derived shadow needed. This class exists to make it easier
	to perform work before the actual shadow class is actually instantiated.
	For example, you could fix InitJobAd() to get the job ad from a file, etc.
	*/

class ShadowInitializer : public Service
{
	public: /* functions */
		ShadowInitializer(int argc, char *argv[]);
		~ShadowInitializer(){};
		
		/* This function begins the process of setting up handlers and doing
			anything that you'd like before the actual shadow gets
			instantiated. This is the entry point to the whole enchilada
			and usually what you'd call in a main_init() context. */
		void Bootstrap(void);

	private: /* functions */
		/* This figures out where to get a job ad and the methods of getting
			one. Only one of these methods should occur and each method
			should check to see if the other method has already done the work.
			If this is the case, then the method to go off second should do 
			nothing.
			This method does these steps: 
				1. Register a safesock command handler that waits for
					the schedd to send it the job ad and then calls the
					ShadowInitialize() function.
				2. Register a Timer that uses ConnectQ() to get 
					the jobad the old fashioned way and then calls the 
					ShadowInitialize() function. */
		void InitJobAd(void);

		/* This function looks into the job ad once it has been set up, and
			performs any other actions necessary before creating the 
			desired shadow and calling the shadow's initializor functions. */
		void ShadowInitialize(void);
			
		/* The command handler that waits for the schedd's UDP job ad packet */
		int AcceptJobAdFromSchedd(int command, Stream *s);
		/* The timer that wakes up and gets the JobAd from the schedd */
		int AcquireJobAdFromSchedd(void);

		/* I can't be copied, (for now at least) */
		ShadowInitializer(const ShadowInitializer&);
		ShadowInitializer& operator=(const ShadowInitializer&);

	private: /* variables */
		ClassAd *m_jobAd; /* gets handed off to the real shadow */
		int m_argc;
		char **m_argv;	/* gets handed off to real shadow */
		int m_accept_id;
		int m_acquire_id;
};

#endif


